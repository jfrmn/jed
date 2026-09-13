#include "tool-output.hh"
#include "settings.hh"
#include "util.hh"
#include "logging.hh"
#include "tools.hh"

#include "util/diagnostics.hh"
#include "ui/constants.h"
#include "ui/window.hh"
#include "ui/animation.hh"
#include "graphics.hh"

#include <charconv>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Init
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool ToolOutput::Init() {
	scrollarea.barWidth = SCROLLBAR_WIDTH_WIDE;
	filePreview.Init();
	return true;
}


bool ToolOutput::IsOpen() const {
	return open || spawnAnimationValue < 1.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Command compiling
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void AppendParameterValue(std::string* builder, const ParameterValue& value, const Parameter& definition) {
	switch (definition.type) {
		case Parameter::Type_None: break;
		case Parameter::Type_String: {
			builder->append(value.stringValue);
		} break;
		case Parameter::Type_Enum: {
			ASSERT(value.enumIndex < definition.enumValues.size());
			builder->append(definition.enumValues[value.enumIndex].GetValue());
		} break;
		case Parameter::Type_Number: {
			char buffer[32] {'\0'};
			const auto result = std::to_chars(buffer, buffer+16, value.numberValue, 10);
			ASSERT(result.ec != std::errc());
			
			builder->append(buffer, result.ptr);
		} break;
		case Parameter::Type_Bool: {
			if (value.boolValue) {
				if (definition.hasIfTrue)
					builder->append(definition.ifTrue);
				else 
					builder->append("true");
			} else {
				if (definition.hasIfFalse)
					builder->append(definition.ifFalse);
				else 
					builder->append("false");
			}
		} break;
		default: ASSERT_UNREACHABLE;
	};
}

static bool CompileCommand(ToolOutput* self, /*out*/ std::string* commandLine) {
	if (self->toolParameterValues.size() < self->tool->parameters.size()) {
		self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
			.type = ToolOutput::ToolDiagnosticsRecord::Type_Command,
			.message = FormatString("Not enough parameters provided. Expected %u but got %u", self->toolParameterValues.size(), self->tool->parameters.size())});
		return false;
	}
		
	//
	// repalce parameters
	//	
	for (u64 pos, start = 0u; /**/; start = pos) {
		pos = self->tool->command.find('%', start);
		if (pos == std::string::npos) {
			// append remaining command
			commandLine->append(self->tool->command, start);
			break;
		}
		
		// append everything up to the percent
		commandLine->append(self->tool->command, start, (pos - start));
		start = pos+1;
		
		// check if it's an escaped percent - e.g. %%
		if (pos < self->tool->command.size()-1 && self->tool->command[pos+1] == '%') {
			commandLine->push_back('%');
			pos += 2;
			continue;
		}
		
		// check if it's reference to a parameter by index - e.g. %1
		if (pos < self->tool->command.size()-1 && std::isdigit(self->tool->command[pos+1]) != 0) {
			pos += 1;
			
			u64 parameterIndex = U64_MAX;
			const auto fcr = std::from_chars(self->tool->command.data()+pos, self->tool->command.data()+self->tool->command.size(), parameterIndex);
			
			// we just checked if its a numeric char so this should come back as ok
			ASSERT(fcr.ec == std::errc());
			ASSERT(parameterIndex < U64_MAX);
			const u64 foundEndPosition = static_cast<u64>(fcr.ptr - self->tool->command.data());
			
			if (parameterIndex >= self->toolParameterValues.size()) {
				self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
				 	.type    = ToolOutput::ToolDiagnosticsRecord::Type_Command,
				 	.message = FormatString("Parameter with index %u not found (%u parameters defined)", parameterIndex, self->toolParameterValues.size()),
				 	.from    = pos-1,
				 	.to      = foundEndPosition});
				return false;
			}
					
			const ParameterValue& paramValue = self->toolParameterValues[parameterIndex];
			const Parameter& paramDef = self->tool->parameters[parameterIndex];
			AppendParameterValue(commandLine, paramValue, paramDef);
		
			pos = foundEndPosition;
			continue;
		}
		
		// check if it's a reference to a parameter by name - e.g. %(my_param)
		if (pos < self->tool->command.size()-1 && self->tool->command[pos+1] == '(') {
			pos += 2;
			
			const u64 posEnd = self->tool->command.find(')', pos);
			if (posEnd == std::string::npos) {
				self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
					.type     = ToolOutput::ToolDiagnosticsRecord::Type_Command,
					.message  = "Missing closing ')' for parameter-reference by name",
					.from     = pos-2,
					.to       = self->tool->command.size()});
				return false;
			}
			
			const std::string_view parameterName {self->tool->command.data() + pos, self->tool->command.data() + posEnd};
			
			const Parameter* paramDef = nullptr;
			const ParameterValue* paramValue = nullptr;
			for (u64 i = 0; i < self->tool->parameters.size(); i++) {
				if (self->tool->parameters[i].name == parameterName) {
					paramDef = &self->tool->parameters[i];
					paramValue = &self->toolParameterValues[i];
					goto found;
				}
			}
			
			self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
				.type     = ToolOutput::ToolDiagnosticsRecord::Type_Command,
				.message  = FormatString("Parameter with name '%.*s' not found", SIZE_AND_DATA(parameterName)),
				.from     = pos-2,
				.to       = posEnd});
			return false;
			
		found:
			AppendParameterValue(commandLine, *paramValue, *paramDef);
			pos = posEnd+1;
		}
	}
	
	//
	// check capture groups
	//
	{
		auto funcCheckCaptureGroups = [self] (std::string_view name, const Regex& regex, u32 requestedGroup) {
			if (!regex.isOk) return;
			if (requestedGroup == U32_MAX) return;
			if (requestedGroup < regex.TotalCaptureGroupCount()) return;
			
			self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
				.type    = ToolOutput::ToolDiagnosticsRecord::Type_Command,
				.message = FormatString("capture group '%.*s' (index %u) is out of range. Regex provided only %u capture groups", SIZE_AND_DATA(name), requestedGroup, regex.additionalCaptureGroupCount+1u)});
		};
		
		funcCheckCaptureGroups("progress.group-value",    self->tool->progress.regex,    self->tool->progress.captureGroupValue);
		funcCheckCaptureGroups("progress.group-max",      self->tool->progress.regex,    self->tool->progress.captureGroupMax);
		funcCheckCaptureGroups("diagnostics.group-file",  self->tool->diagnostics.regex, self->tool->diagnostics.captureGroupFile);
		funcCheckCaptureGroups("diagnostics.group-line",  self->tool->diagnostics.regex, self->tool->diagnostics.captureGroupLine);
		funcCheckCaptureGroups("diagnostics.group-color", self->tool->diagnostics.regex, self->tool->diagnostics.captureGroupColor);
	}
	
	return true;
}

bool ToolOutput::StartProcess() {
	
	if (process && process->IsRunning()) {
		LogError("a process already running");
		return false;
	}
	
	toolDiagnostics.clear();
	selectedDiagnosticsRecord = U64_MAX;
	
	progressValue = 0.0f;
	progressText.clear();
	
	diagnosticsRecords.clear();
	
	delete process;
	process = nullptr;
	
	styleChanges.clear();
	lines.clear();
	
	glyphRunCacheIsValid = false;
	glyphRunCache.clear();
	
	selectionStart = selectionEnd = TextPosition {};
	
	
	std::string commandLine {};
	if (!CompileCommand(this, &commandLine)) {
		open = true;
		return false;
	}
	
	LogInfo("Running: %s", commandLine.c_str());
	
	Process::StartInfo startInfo {
		.application = {},
		.commandLine = std::move(commandLine),
		.environment = tool->environment,
		.flags = tool->flags};
	
	ASSERT(!process);
	process = new Process();
	process->observer = this;
	
	if (!process->Start(std::move(startInfo))) {
		delete process;
		process = nullptr;
		toolDiagnostics.clear();
		toolDiagnostics.push_back(ToolDiagnosticsRecord {
			.type    = ToolDiagnosticsRecord::Type_Command,
			.message = FormatString("Failed to start process. Last Error: %s", StrLastErr(GetLastError()))});
		open = true;
		return false;
	}
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Update
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static f32 ToolbarHeight() {
	return MARGIN_X2 + settings.fontUi.lineHeight;
}

static void UpdateFilePreview(ToolOutput* self, const ToolOutput::EditorDiagnosticsRecord& record) {
	self->selectionStart = TextPosition {record.originLine, record.originFromColumn};
	self->selectionEnd   = TextPosition {record.originLine, record.originToColumn};
	self->filePreview.Load(FilePreview::LoadArgs {
		.path = record.file,
		.mode = FilePreview::LoadMode_TargetLine,
		.targetLine = record.line,
		.hasSelection = true,
		.selectionFrom = self->selectionStart,
		.selectionTo = self->selectionEnd});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnOpenConfigurator(void* ud, u64) {}

static void OnClickedKillProcess(void* ud, u64) {
	auto self = static_cast<ToolOutput*>(ud);
	
	ASSERT(self->process);
	self->process->Terminate();
}

static void OnRerunProcess(void* ud, u64) {
	auto self = static_cast<ToolOutput*>(ud);
	ASSERT_NOT_IMPLEMENTED;
}

static void OnClickToolDiagnostics(void* ud, u64) {
	auto self = static_cast<ToolOutput*>(ud);
	// should open the source of the tool or something
	ASSERT_NOT_IMPLEMENTED;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ToolOutput::Update() {

	//
	// advance animation
	//
	AnimationLinear::Advance(&spawnAnimationValue, 0.008f);
	
	const f32 width = open
		? spawnAnimationValue * RectWidth(area)
		: (1.0f - spawnAnimationValue) * RectWidth(area);
	
	const D2D_RECT_F animatedArea {
		.left   = area.right - width,
		.top    = area.top,
		.right  = area.right,
		.bottom = area.bottom};

	//
	// draw backgound
	//
	{
		ID2D1Bitmap* bitmap = CopyFromRenderTarget(deviceContext, animatedArea);
		if (!bitmap) return;
		DrawGlow(deviceContext, bitmap, animatedArea);
		BlurArea(deviceContext, animatedArea, bitmap);	
		bitmap->Release();
	}
	
	const std::scoped_lock lock {mtx};
	const f32 toolbarHeight = ToolbarHeight();
		
	//
	// reshape glyphs
	//	
	if (!glyphRunCacheIsValid) {
		glyphRunCache.clear();
		for (const std::string& line : lines) {
			GlyphRun& run = glyphRunCache.emplace_back();
			run.Shape(line, settings.fontEditor);
		}
		
		glyphRunCacheIsValid = true;
	}
	
	//
	// update scrollarea (but don't render yet)
	//
	{
		scrollarea.totalSize = D2D_SIZE_F {
			.width  = RectWidth(area),
			.height = glyphRunCache.size() * settings.fontEditor.lineHeight};
		
		if (!disableAutoScroll)
			scrollarea.vpY = scrollarea.GetMaxPositionY();
	}
		
	//
	// render lines
	//
	{
		const f32 toolbarHeight = ToolbarHeight();

		//
		// prepare offscreen render targets
		//
		const D2D_SIZE_F areaSize {
			.width  = RectWidth(animatedArea),
			.height = RectHeight(animatedArea) - toolbarHeight};
		
		ID2D1BitmapRenderTarget* foreground = CreateCompatibleRenderTarget(deviceContext, areaSize);
		if (!foreground) return;
		DEFER(foreground->Release());
		
		ID2D1BitmapRenderTarget* text = CreateCompatibleRenderTarget(deviceContext, areaSize);
		if (!text) return;
		DEFER(text->Release());
		
		ID2D1RenderTarget* background = deviceContext;
		
		foreground->BeginDraw();
		foreground->Clear(settings.colors.editorText.ToD2D());
	
		text->BeginDraw();
		text->Clear();
		
		//
		// draw styles
		//	
		if (!styleChanges.empty()) {
		
			background->SetTransform(D2D1::Matrix3x2F::Translation(area.left, area.top + toolbarHeight));
			DEFER(background->SetTransform(D2D1::Matrix3x2F::Identity()));
			
			foreground->SetTransform(D2D1::Matrix3x2F::Translation(scrollarea.vpX, -scrollarea.vpY));
			
			ID2D1SolidColorBrush* brushForeground = nullptr;
			foreground->CreateSolidColorBrush(settings.colors.editorText.ToD2D(), &brushForeground);
			if (!brushForeground) return;
			DEFER(brushForeground->Release());
			
			ID2D1SolidColorBrush* brushBackground = nullptr;
			background->CreateSolidColorBrush(D2D_COLOR_F {0.0f, 0.0f, 0.0f, 0.0f}, &brushBackground);
			if (!brushBackground) return;
			DEFER(brushBackground->Release());
			
			struct {
				bool hasBackgroundColor = false;
				bool hasForegroundColor = false;
				bool hasUnderline = false;
				bool hasNegative = false;
			} state;
			TextPosition start = {0u, 0u};
			
			const auto ApplyStyle = [&] (u64 ln, u64 fromCp, u64 toCp) {
				const GlyphRun& run = glyphRunCache[ln];
									
				f32 from = .0f, to = .0f;
				run.MeasureOffsetRange(fromCp, toCp, &from, &to);
				
				const D2D_RECT_F rect {
					.left   = PADDING + from,
					.top    = ln * settings.fontEditor.lineHeight,
					.right  = PADDING + to,
					.bottom = (ln+1) * settings.fontEditor.lineHeight};
			
				if (state.hasBackgroundColor)
					background->FillRectangle(rect, (state.hasNegative ? brushForeground : brushBackground));
					
				if (state.hasForegroundColor)
					foreground->FillRectangle(rect, (state.hasNegative ? brushBackground : brushForeground));
					
				if (state.hasUnderline)
					text->DrawLine(
						D2D_POINT_2F {.x = PADDING + rect.left,  .y = toolbarHeight + rect.top + settings.fontEditor.baselineOffset},
						D2D_POINT_2F {.x = PADDING + rect.right, .y = toolbarHeight + rect.top + settings.fontEditor.baselineOffset},
						alphaMaskBrush);
			};
			
			for (u64 i = 0u; i < styleChanges.size(); i++) {
				
				const ToolOutput::StyleChange* styleChange = &styleChanges[i];
				
				IterateTextRange(start, styleChange->position, ApplyStyle);
				
				// do the style change
				switch (styleChange->type) {
					case ToolOutput::StyleChangeType_Bold: break; // @TODO
					case ToolOutput::StyleChangeType_Underline: state.hasUnderline = styleChange->value; break;
					case ToolOutput::StyleChangeType_Negative: {
						state.hasNegative = styleChange->value;
					} break;
					case ToolOutput::StyleChangeType_Foreground: {
						brushForeground->SetColor(styleChange->color.ToD2D());
						state.hasForegroundColor = true;
					} break;
					case ToolOutput::StyleChangeType_ForegroundDefault: {
						state.hasForegroundColor = false;
					} break;
					case ToolOutput::StyleChangeType_Background: {
						brushBackground->SetColor(styleChange->color.ToD2D());
						state.hasBackgroundColor = true;
					} break;
					case ToolOutput::StyleChangeType_BackgroundDefault: {
						state.hasBackgroundColor = false;
					} break;
					case ToolOutput::StyleChangeType_Reset: {
						state.hasUnderline = false;
						state.hasNegative = false;
						state.hasForegroundColor = false;
						state.hasBackgroundColor = false;
					} break;
					default: break;
				}
				start = styleChange->position;
			}
		}
		
		//
		// draw glyph runs
		//
		{
			text->SetTransform(D2D1::Matrix3x2F::Translation(scrollarea.vpX, -scrollarea.vpY));
			
			for (u64 i = 0; i < glyphRunCache.size(); i++) {
				const GlyphRun& run = glyphRunCache[i];
				run.Draw(text, PADDING , (i * settings.fontEditor.lineHeight), settings.fontEditor, alphaMaskBrush);
			}
		}
		
		foreground->EndDraw();
		text->EndDraw();
		
		//
		// blend images
		//
		{
			ID2D1Bitmap* bmForeground, *bmText;
			foreground->GetBitmap(&bmForeground);
			text->GetBitmap(&bmText);
			
			//const D2D_RECT_F dest {.left = area.left, .top = area.top, .right = }
			//deviceContext->DrawBitmap(bmForeground, &area);
			BlendImages(deviceContext, {area.left, area.top + toolbarHeight}, bmForeground, bmText);
			
			bmForeground->Release();
			bmText->Release();
		}
	}
		
	
	// we need to draw the tooltip outside of the clip rect
	const ToolDiagnosticsRecord* toolDiagnosticsRecordUnderCursor = nullptr;
	
	//
	// draw diagnostics + selection
	//
	{
		deviceContext->PushAxisAlignedClip(
			D2D_RECT_F {
				.left   = animatedArea.left,
				.top    = animatedArea.top + toolbarHeight,
				.right  = animatedArea.right,
				.bottom = animatedArea.bottom},
			D2D1_ANTIALIAS_MODE_ALIASED);
		DEFER(deviceContext->PopAxisAlignedClip());
	
		//
		// draw matched diagnostics
		//
		for (u64 i = 0; i < diagnosticsRecords.size(); i++) {
			const EditorDiagnosticsRecord& record = diagnosticsRecords[i];
			
			ASSERT(record.originLine < glyphRunCache.size());
			const GlyphRun& run = glyphRunCache[record.originLine];
			
			ASSERT(record.originFromColumn < record.originToColumn);
			
			f32 offsetFrom = .0f, offsetTo = .0f;
			run.MeasureOffsetRange(record.originFromColumn, record.originToColumn, &offsetFrom, &offsetTo);
			
			deviceContext->DrawRectangle(
				D2D_RECT_F {
					.left   = animatedArea.left + PADDING + offsetFrom,
					.top    = animatedArea.top  + toolbarHeight + ( record.originLine    * settings.fontEditor.lineHeight) - scrollarea.vpY,
					.right  = animatedArea.left + PADDING + offsetTo,
					.bottom = animatedArea.top  + toolbarHeight + ((record.originLine+1) * settings.fontEditor.lineHeight) - scrollarea.vpY},
				UseColor(record.color));
		}
	
		//
		// draw tool diagnostics
		//
		for (u64 i = 0; i < toolDiagnostics.size(); i++) {
			const ToolDiagnosticsRecord& record = toolDiagnostics[i];
			if (record.type == ToolDiagnosticsRecord::Type_Command) continue;
			
			ASSERT(record.line < glyphRunCache.size());
			const GlyphRun& run = glyphRunCache[record.line];
			
			ASSERT(record.from < record.to);
			
			f32 offsetFrom = .0f, offsetTo = .0f;
			run.MeasureOffsetRange(record.from, record.to, &offsetFrom, &offsetTo);
			
			const D2D_RECT_F areaRecord {
				.left   = animatedArea.left + PADDING + offsetFrom,
				.top    = animatedArea.top + toolbarHeight + (record.line *  settings.fontEditor.lineHeight) - scrollarea.vpY,
				.right  = animatedArea.left + PADDING + offsetTo,
				.bottom = animatedArea.top + toolbarHeight + (record.line * (settings.fontEditor.lineHeight+1)) - scrollarea.vpY};
			
			deviceContext->DrawLine(
				D2D_POINT_2F {
					.x = areaRecord.left,
					.y = areaRecord.top + settings.fontEditor.underlineOffset},
				D2D_POINT_2F {
					.x = areaRecord.right,
					.y = areaRecord.top + settings.fontEditor.underlineOffset},
				UseColor(COLOR_YELLOW),
				2.0f,
				strokeStyleDashed);
				
			if (mouse.Hittest(areaRecord, this, OnClickToolDiagnostics, i)) 
				toolDiagnosticsRecordUnderCursor = &record;
		}
	
		//
		// draw selection
		//
		if (selectionStart != selectionEnd) {
			
			const TextPosition* from = nullptr, *to  = nullptr;
			if (selectionStart < selectionEnd) from = &selectionStart, to = &selectionEnd;
			else from = &selectionEnd, to = &selectionStart;
			
			IterateTextRange(*from, *to, [this, toolbarHeight](u64 ln, u64 fromCp, u64 toCp) {
				const GlyphRun& run = glyphRunCache[ln];
									
				f32 offsetFrom = .0f, offsetTo = .0f;
				run.MeasureOffsetRange(fromCp, toCp, &offsetFrom, &offsetTo);
				
				deviceContext->FillRectangle(
					D2D_RECT_F {
						.left   = area.left + PADDING + offsetFrom,
						.top    = area.top  + toolbarHeight + (settings.fontEditor.lineHeight * ln)     - scrollarea.vpY,
						.right  = area.left + PADDING + offsetTo,
						.bottom = area.top  + toolbarHeight + (settings.fontEditor.lineHeight * (ln+1)) - scrollarea.vpY},
					settings.GetBrushSelection());
			});
		}
	}
			
	//
	// hittest to change selection
	//
	if (!lines.empty()) {
		const D2D_RECT_F outputArea {
			.left   = animatedArea.left,
			.top    = animatedArea.top + toolbarHeight,
			.right  = animatedArea.right,
			.bottom = animatedArea.bottom};
		
		if (mouse.Hittest(outputArea, this, nullptr)) {
		
			const D2D_POINT_2F relativePoistion {mouse.x - outputArea.left, mouse.y - outputArea.top};
			const u64 hitLine = std::clamp(
				static_cast<u64>((relativePoistion.y + scrollarea.vpY) / settings.fontEditor.lineHeight),
				0ull, 
				lines.size()-1u);
			
			ASSERT(hitLine < lines.size());
			ASSERT(hitLine < glyphRunCache.size());
			
			const GlyphRun& hitRun = glyphRunCache[hitLine];
			const u64 hitColumn    = hitRun.HitTest(relativePoistion.x);
			
			if (mainWindow.event.type == Event::Type_MouseDown) {				
				
				mouse.StartDragging();
				selectionStart = selectionEnd = TextPosition {hitLine, hitColumn};
				selectedDiagnosticsRecord = U64_MAX;
			
			} else if (mainWindow.event.type == Event::Type_MouseUp && selectionStart == selectionEnd) {
				
				// check if we hit a matched diagnostic record
				for (u64 i = 0u; i < diagnosticsRecords.size(); i++) {
					const ToolOutput::EditorDiagnosticsRecord& record = diagnosticsRecords[i];
					
					const bool hitThisRecord = record.originLine == hitLine &&
											   record.originFromColumn <= hitColumn &&
											   record.originToColumn >= hitColumn;
					if (hitThisRecord) {
						UpdateFilePreview(this, record);
						selectedDiagnosticsRecord = i;
						break;
					}
				}
			
			} else if (mouse.isDragging) {
				selectionEnd = TextPosition {hitLine, hitColumn};
			}
		}
	}
	
	//
	// draw tooltip for tool diagnostics
	//
	if (toolDiagnosticsRecordUnderCursor) {
		
		ASSERT(toolDiagnosticsRecordUnderCursor->line < glyphRunCache.size());
		const GlyphRun& run = glyphRunCache[toolDiagnosticsRecordUnderCursor->line];
		
		ASSERT(toolDiagnosticsRecordUnderCursor->from < toolDiagnosticsRecordUnderCursor->to);
		
		staticGlyphRun.Shape(toolDiagnosticsRecordUnderCursor->message, settings.fontUi);
		
		const D2D_RECT_F tooltipArea = MakeRect(
			mouse.x - staticGlyphRun.width - PADDING_X2,
			mouse.y + PADDING_X2,
			PADDING_X2 + staticGlyphRun.width,
			PADDING_X2 + settings.fontUi.lineHeight);
		deviceContext->FillRoundedRectangle(ToRounded(tooltipArea), UseColor(settings.colors.uiBackground));
		deviceContext->DrawRoundedRectangle(ToRounded(tooltipArea), UseColor(settings.colors.dropShadow));
		
		staticGlyphRun.Draw(deviceContext, mouse.x - staticGlyphRun.width - PADDING, mouse.y + PADDING_X3, settings.fontUi, UseColor(settings.colors.uiText));
	}
	
	//
	// update file preview
	//
	if (selectedDiagnosticsRecord != U64_MAX) {
		const ToolOutput::EditorDiagnosticsRecord& record = diagnosticsRecords[selectedDiagnosticsRecord];
	
		filePreview.x = animatedArea.left - filePreview.width;
		filePreview.y = animatedArea.top  + toolbarHeight + ((record.originLine-2u) * settings.fontEditor.lineHeight) - scrollarea.vpY;
		filePreview.OnUpdate();
			
		deviceContext->DrawRectangle(filePreview.GetArea(), UseColor(record.color));
	}
	
	//
	// scrollarea
	//
	scrollarea.OnUpdate();
	
	//
	// draw toolbar
	//
	{
		const D2D_RECT_F toolbarArea {
			.left   = animatedArea.left,
			.top    = animatedArea.top,
			.right  = animatedArea.right,
			.bottom = animatedArea.top + toolbarHeight};
		
		deviceContext->FillRectangle(toolbarArea, settings.GetBrushUiBackground());
	
		if (tool) {
			f32 offsetX = 0.0f;
						
			// draw tool name
			{
				staticGlyphRun.Shape(tool->name, settings.fontUi);
				staticGlyphRun.Draw(deviceContext, animatedArea.left + MARGIN, animatedArea.top + MARGIN, settings.fontUi, settings.GetBrushUiText());
				
				offsetX = staticGlyphRun.width + MARGIN_X2;
				
				const D2D_RECT_F toolNameArea {
					.left   = toolbarArea.left,
					.top    = toolbarArea.top,
					.right  = toolbarArea.left + offsetX,
					.bottom = toolbarArea.bottom};
			}
			
			constexpr f32 PROGRESS_AREA_WIDTH = 200.0f;
			const D2D_RECT_F progressArea {
				.left   = toolbarArea.left + offsetX,
				.top    = toolbarArea.top + MARGIN - PADDING,
				.right  = toolbarArea.left + offsetX + PROGRESS_AREA_WIDTH,
				.bottom = toolbarArea.bottom - MARGIN + PADDING};
								
			// draw progress bar
			if (tool->progress.regex.isOk) {
				
				deviceContext->FillRoundedRectangle(ToRounded(
					MakeRect(progressArea.left, progressArea.top, (PROGRESS_AREA_WIDTH * progressValue), RectHeight(progressArea))),
					UseColor(Color::FromKnown(D2D1::ColorF::Green)));
				
				deviceContext->DrawRoundedRectangle(ToRounded(
					progressArea),
					settings.GetBrushUiText());
			} else {
				deviceContext->FillRoundedRectangle(ToRounded(
					progressArea),
					settings.GetBrushUiBackground(false));
			}
				
			// draw prgress text
			{
				std::string_view label = progressText;
				std::string_view hoverLabel;
				MouseState::Callback onClickFunc;
				Color labelColor, hoverLabelColor;
				
				char exitCodeBuffer[32] {'\0'};
				const u64 exitCode = process ? process->GetExitCode() : 0;
				
				if (!process) {
					onClickFunc = OnRerunProcess;
					label = "Error";
					hoverLabel = "Retry";
					labelColor = Color::FromKnown(D2D1::ColorF::Red);
					hoverLabelColor = settings.colors.uiText;
				
				} else if (exitCode == STILL_ACTIVE) {
					onClickFunc = OnClickedKillProcess;
					labelColor = settings.colors.uiText;
					hoverLabel = "Terminate";
					hoverLabelColor = Color::FromKnown(D2D1::ColorF::Crimson);
				
				} else {
					onClickFunc = OnRerunProcess;
					labelColor = (exitCode == 0)
						? settings.colors.uiText
						: Color::FromKnown(D2D1::ColorF::Crimson);
					hoverLabel = "Restart";
					hoverLabelColor = settings.colors.uiText;
				}
				
				if (mouse.Hittest(progressArea, this, onClickFunc)) {
					deviceContext->FillRoundedRectangle(ToRounded(progressArea), settings.GetBrushHover(mouse.isDown));
					
					brush->SetColor(hoverLabelColor.ToD2D());
					staticGlyphRun.Shape(hoverLabel, settings.fontUi);
				
				} else {
					brush->SetColor(labelColor.ToD2D());
					staticGlyphRun.Shape(label, settings.fontUi);
				}
				
				const f32 x = animatedArea.left + offsetX + (PROGRESS_AREA_WIDTH / 2.0f) - (staticGlyphRun.width / 2.0f);
				staticGlyphRun.Draw(deviceContext, x, animatedArea.top + MARGIN, settings.fontUi, brush);
				
				offsetX += PROGRESS_AREA_WIDTH + MARGIN;
			}
			
			// draw warning icon
			if (!toolDiagnostics.empty()) {
				const D2D_RECT_F areaWarningIcon {
					.left   = toolbarArea.left + offsetX,
					.top    = toolbarArea.top + MARGIN - PADDING,
					.right  = toolbarArea.left + offsetX + settings.fontUi.lineHeight + PADDING_X2,
					.bottom = toolbarArea.bottom - MARGIN + PADDING};
				
				deviceContext->DrawBitmap(
					settings.icons.editorDiagnosticsWarning,
					MakeRect(area.left + offsetX + PADDING, area.top + MARGIN, settings.fontUi.lineHeight, settings.fontUi.lineHeight));
					
				if (mouse.Hittest(areaWarningIcon, this, OnClickToolDiagnostics)) {
					
					// @FIXME we do this every frame. Performance...
					std::vector<GlyphRun> toolDiagnosticsRuns {};
					f32 longestLine = 0.0f;
					for (const ToolDiagnosticsRecord& record : toolDiagnostics) {
					//	if (record.type != ToolDiagnosticsRecord::Type_Command) continue;
						
						GlyphRun& run = toolDiagnosticsRuns.emplace_back();
						run.Shape(record.message, settings.fontUi);
						
						if (longestLine < run.width)
							longestLine = run.width;
					}
					
					const D2D_RECT_F areaTooltip = MakeRect(
						mouse.x - longestLine - PADDING_X2,
						mouse.y + PADDING_X3,
						PADDING_X2 + longestLine,
						PADDING_X2 + (toolDiagnosticsRuns.size() * settings.fontUi.lineHeight));
					deviceContext->FillRoundedRectangle(ToRounded(areaTooltip), UseColor(settings.colors.uiBackground));
					deviceContext->DrawRoundedRectangle(ToRounded(areaTooltip), UseColor(settings.colors.dropShadow));
					
					for (const GlyphRun& run : toolDiagnosticsRuns)
						run.Draw(deviceContext, areaTooltip.left + PADDING, areaTooltip.top + PADDING, settings.fontUi, UseColor(settings.colors.uiText));
				}
				
				offsetX += settings.fontUi.lineHeight + PADDING_X2;
			}
			
		// no tool
		} else {
			staticGlyphRun.Shape("No tool run yet.", settings.fontUi);
			staticGlyphRun.Draw(deviceContext, animatedArea.left + MARGIN, animatedArea.top + MARGIN, settings.fontUi, settings.GetBrushUiText(false));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void ToolOutput::OnResize(f32 newWidth, f32 newHeight) {
	area = D2D_RECT_F {
		.left = std::floor(mainWindow.width * 0.6f),
		.top = PADDING_X2 + settings.fontUi.lineHeight,
		.right = mainWindow.width,
		.bottom = mainWindow.height - PADDING_X2 - settings.fontUi.lineHeight};
	
	scrollarea.position = D2D_POINT_2F {
		.x = area.left,
		.y = area.top + ToolbarHeight()};	
	scrollarea.vpSize = D2D_SIZE_F {
		.width = RectWidth(area),
		.height = RectHeight(area) - ToolbarHeight()};
}

void ToolOutput::OnMouseWheel(f32 distance) {
	// @TODO(settings) scroll distance
	scrollarea.ScrollVertical(distance * settings.fontEditor.lineHeight * 5);
	disableAutoScroll = (scrollarea.vpY != scrollarea.GetMaxPositionY());	
}

bool ToolOutput::HandleEvent(const Event& event) {
	if (event.type != Event::Type_Command) return false;
	
	if (event.cmd.id == Command::Id_GotoNextDiagnosticRecord) {
		selectedDiagnosticsRecord = IncrementWrapAround(selectedDiagnosticsRecord, diagnosticsRecords.size());
		UpdateFilePreview(this, diagnosticsRecords[selectedDiagnosticsRecord]);
		return true;
		
	} else if (event.cmd.id == Command::Id_GotoPrevDiagnosticRecord) {
		selectedDiagnosticsRecord = DecrementWrapAround(selectedDiagnosticsRecord, diagnosticsRecords.size());
		UpdateFilePreview(this, diagnosticsRecords[selectedDiagnosticsRecord]);
		return true;
	
	} else if (event.cmd.id == Command::Id_ToolOutput_TerminateProcess) {
		if (process) process->Terminate();
		return true;
	
	} else if (event.cmd.id == Command::Id_Clipboard_Copy) {
		if (selectionStart == selectionEnd) return false; // we could consume the command or not. Up for debate...
		
		const std::string& startLine = lines[selectionStart.line];
		const std::string& endLine   = lines[selectionEnd.line];
		
		OpenClipboard(mainWindow.hWnd);
		DEFER(CloseClipboard());
		
		EmptyClipboard();
		
		u64 totalSize = 0u;
		IterateTextRange(selectionStart, selectionEnd, [&] (u64 ln, u64 from, u64 to) {
			if (to == U64_MAX)
				to = lines[ln].size();
			totalSize += (to - from);
		});
		
		HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, totalSize + 1u);
		char* mem = static_cast<char*>(GlobalLock(hGlobal));
		
		u64 alreadyCopied = 0u;
		IterateTextRange(selectionStart, selectionEnd, [&] (u64 ln, u64 from, u64 to) {
			const std::string_view line = lines[ln];
			if (to == U64_MAX)
				to = line.size();
			
			const u64 cnt = to - from;
			memcpy_s(mem + alreadyCopied, totalSize - alreadyCopied, line.data() + from, cnt);
			alreadyCopied += cnt;
		});
		
		GlobalUnlock(hGlobal);
		SetClipboardData(CF_TEXT, hGlobal);
		return true;
	}
	
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Output processing
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void PushFailedToParseDiagnostics(ToolOutput* self, std::string_view groupName, std::from_chars_result fcr, u64 from, u64 to) {
	ASSERT(!self->lines.empty());
	self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
		.type     = ToolOutput::ToolDiagnosticsRecord::Type_Output,
		.message  = FormatString("failed to parse matched text for capture group '%.*s': %s", SIZE_AND_DATA(groupName), Str(fcr)),
		.from     = from,
		.to       = to,
		.line     = self->lines.size()-1u});
}

static void MatchProgress(ToolOutput* self, const std::string* line) {
	if (!self->tool->progress.regex.isOk) return;
	
	RegexMatch match;
	if (!self->tool->progress.regex.Match(*line, &match)) return;
	if (self->tool->progress.captureGroupValue >= match.groupCount) return;
	
	s64 newValue = 0;
	const RegexMatch::Group groupValue = match.GetGroup(self->tool->progress.captureGroupValue);
	
	const std::from_chars_result fcrValue = std::from_chars(groupValue.begin, groupValue.end, newValue);
	if (fcrValue.ec != std::errc()) {
		PushFailedToParseDiagnostics(self, "progress.group-value", fcrValue, groupValue.begin-line->data(), groupValue.end-line->data());
		return;
	}
		
	s64 newMax = self->tool->progress.maxValue;
	if (self->tool->progress.captureGroupMax < match.groupCount) {
		const RegexMatch::Group groupMax = match.GetGroup(self->tool->progress.captureGroupMax);
		
		const std::from_chars_result fcrMax = std::from_chars(groupMax.begin, groupMax.end, newMax);
		if (fcrMax.ec != std::errc()) {
			PushFailedToParseDiagnostics(self, "progress.group-max", fcrMax, groupMax.begin-line->data(), groupMax.end-line->data());
			// not aborting, using the default max
		}
	}
	
	self->progressValue = static_cast<f32>(newValue) / newMax;
	if (self->tool->progress.format == Tool::Progress::Format_None) {
		// nothing
	
	} else if (self->tool->progress.format == Tool::Progress::Format_Percent) {
		constexpr u64 bufferSize = 16;
		char buffer[bufferSize] {'\0'};
		
		const std::to_chars_result tcr = std::to_chars(
			buffer,
			buffer+bufferSize-1, // -1 so we have space for the %
			(self->progressValue * 100.0f),
			std::chars_format::fixed, 2);
		
		if (tcr.ec == std::errc()) {
			*tcr.ptr = '%';
			self->progressText.assign(buffer, tcr.ptr + 1);
			
		} else if (tcr.ec == std::errc::value_too_large) {
			self->progressText.assign("TOO LARGE");
			
		} else {
			LogWarning("converting to precentage text failed. Error: %s", Str(tcr));
			self->progressText.assign("ERROR");
		}
	
	} else if (self->tool->progress.format == Tool::Progress::Format_Absolute) {
		self->progressText.assign(groupValue.GetText());
		self->progressText.append(" / ");
		
		if (self->tool->progress.captureGroupMax < match.groupCount) {
			self->progressText.append(match.GetGroupText(self->tool->progress.captureGroupMax));
		
		} else {
			constexpr u64 bufferSize = 16;
			char buffer[bufferSize] {'\0'};
			
			const std::to_chars_result resultToCh = std::to_chars(buffer, buffer+bufferSize, self->tool->progress.maxValue);
			if (resultToCh.ec == std::errc()) {
				self->progressText.append(buffer, resultToCh.ptr);
			
			} else {
				LogWarning("converting to max text failed. Error: %s", Str(resultToCh));
				self->progressText.append("ERROR");
			}
		}
	
	} else {
		ASSERT_UNREACHABLE;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void MatchDiagnostics(ToolOutput* self, const std::string* line) {
 	if (!self->tool->diagnostics.regex.isOk) return;
	 
	RegexMatch match {};
	if (!self->tool->diagnostics.regex.Match(*line, &match)) return;

	ToolOutput::EditorDiagnosticsRecord record {};
	//record.color = settings.colors.editorText;
	record.originLine = self->lines.size() - 1u;
	
	const RegexMatch::Group fullMatch = match.GetFullMatch();
	record.originFromColumn = fullMatch.begin - line->data();
	record.originToColumn = fullMatch.end - line->data();
	
	// I'm usually not a fan using lambdas this way
	// but this makes this code so much nicer
	
	// color	
	record.color = ([&]() -> Color {
		if (self->tool->diagnostics.captureGroupColor >= match.groupCount)
			return settings.colors.editorText;
		
		const RegexMatch::Group group = match.GetGroup(self->tool->diagnostics.captureGroupColor);	
		for (const Tool::DiagnosticsMatcher::ColorMapping& entry : self->tool->diagnostics.colorMapping) {
			if (StringEqualsCaseInsen(entry.key, group.GetText()))
				return entry.color;
		}
		self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
			.type    = ToolOutput::ToolDiagnosticsRecord::Type_Output,
			.message = FormatString("No mapping for color '%s'"),
			.from    = static_cast<u64>(group.begin - line->data()),
			.to      = static_cast<u64>(group.end - line->data()),
			.line    = self->lines.size() - 1u});
		return settings.colors.editorText;
	})();
		
	// file
	if (self->tool->diagnostics.captureGroupFile < match.groupCount) {
		const RegexMatch::Group group = match.GetGroup(self->tool->diagnostics.captureGroupFile);
		record.file = group.GetText();
	}
	
	// line
	if (self->tool->diagnostics.captureGroupLine < match.groupCount) {
		RegexMatch::Group group = match.GetGroup(self->tool->diagnostics.captureGroupLine);
		const std::from_chars_result fcr = std::from_chars(group.begin, group.end, record.line);
		
		if (fcr.ec != std::errc()) {
			PushFailedToParseDiagnostics(self, "group-line", fcr, group.begin-line->data(), group.end-line->data());
			record.line = 0u;
		}
		
		if (self->tool->diagnostics.linesStartAtOne && record.line > 0u)
			record.line -= 1;
	}
	
	self->diagnosticsRecords.push_back(std::move(record));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static ToolOutput::StyleChange GetStyleChange(int value);

static void ParseChunk(ToolOutput* self, std::string_view data) {

	const std::scoped_lock lock {self->mtx};
	
	ASSERT(!self->lines.empty());
	
	std::string* line = &self->lines.back();
	line->reserve(line->size() + data.size());

	for (u64 i = 0u; i < data.size(); i++) {
		
		if (data[i]  == '\n' || data[i] == '\r') {
			
			MatchProgress(self, line);
			MatchDiagnostics(self, line);
			
			const bool isCrLf = data[i] == '\r' && i < data.length()-1 && data[i+1] == '\n';
		
			line = &self->lines.emplace_back();
			
			if (isCrLf)
				i += 1u;
		
		} else if (data[i] == '\x1b' && i < data.length()-1 && data[i+1] == '[') {
									
			const u64 endOfEscSequence = data.find_first_not_of("0123456789;", i+2);
			
			// invalid escape sequence
			if (endOfEscSequence == std::string_view::npos || data[endOfEscSequence] != 'm')
				continue;
			
			const std::string_view sequence = data.substr(i+2, (endOfEscSequence - i - 2));
			
			// parse style change
			{
				int value = 0;
				const auto result = std::from_chars(sequence.data(), sequence.data() + sequence.length(), value);
				if (result.ec != std::errc()) continue;
				
				u64 j = (result.ptr - data.data());
				
				ToolOutput::StyleChange styleChange = GetStyleChange(value);
				if (styleChange.type == ToolOutput::StyleChangeType_Unknown) continue;
				
				// extended colors
				if (value == 38 || value == 48) {
					if (j >= sequence.length() || sequence[j] != ';') continue;
					j += 1;
					if (j >= sequence.length() || sequence[j] != '2') continue;
					j += 1;
					if (j >= sequence.length() || sequence[j] != ';') continue;
					j += 1;
					
					int r = 0, g = 0, b = 0;	
					std::from_chars_result res {};
					
					res = std::from_chars(sequence.data() + j, sequence.data() + sequence.length(), r);
					if (res.ec != std::errc()) continue;
					j = (res.ptr - sequence.data());
					if (i >= sequence.length() || sequence[i] != ';') continue;
					j += 1;
					
					res = std::from_chars(sequence.data() + j, sequence.data() + sequence.length(), g);
					if (res.ec != std::errc()) continue;
					j = (res.ptr - sequence.data());
					if (i >= sequence.length() || sequence[i] != ';') continue;
					j += 1;
					
					res = std::from_chars(sequence.data() + j, sequence.data() + sequence.length(), b);
					if (res.ec != std::errc()) continue;
					j = (res.ptr - sequence.data());
					
					styleChange.color = Color {r/255.0f, g/255.0f, b/255.0f, 1.0f};
				}
				
				styleChange.position = TextPosition {
					.line = self->lines.size() - 1u,
					.character = line->size()};
				
				self->styleChanges.push_back(styleChange);
				i = endOfEscSequence;
			}			
		
		} else {
			line->push_back(data[i]);
		}
	}
	
	self->glyphRunCacheIsValid = false;
}


void ToolOutput::OnStderr(std::string_view data) {
	ParseChunk(this, data);
	mainWindow.PostUpdate();
}

void ToolOutput::OnStdout(std::string_view data) {
	ParseChunk(this, data);
	mainWindow.PostUpdate();
}

void ToolOutput::OnStarted() {
	std::scoped_lock lock {mtx};
	
	LogInfo("tool '%.*s' started", SIZE_AND_DATA(tool->name));
	
	lines.emplace_back();
	progressText = (tool->progress.format == Tool::Progress::Format_Percent)
		? "0%"
		: "Running";
	
	if (tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnStart && !open) {
		open = true;
		spawnAnimationValue = 1.0f - spawnAnimationValue;
	}
		
	mainWindow.PostUpdate();
}

void ToolOutput::OnExited(int exitCode) {	
	std::scoped_lock lock {mtx};
	
	LogInfo("tool '%.*s' exited with code %d", SIZE_AND_DATA(tool->name), exitCode);
	
	FormatString(&progressText, "Exit %d", exitCode);
	
	const u32 flagToTest = exitCode == 0
		? tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitSuccess
		: tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitError;
	
	if (tool->consoleOpenFlags & flagToTest && !open) {
		open = true;
		spawnAnimationValue = 1.0f - spawnAnimationValue;
	}
		
	mainWindow.PostUpdate();
}

ToolOutput::StyleChange GetStyleChange(int value) {
	switch (value) {
		case 0: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Reset};
		
		// boldness	
		case 1: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Bold,
			.value = true};
			
		case 22: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Bold,
			.value = false};
		
		// underline	
		case 4: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Underline,
			.value = true};
			 
		case 24: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Underline,
			.value = false};
		
		// negative	
		case 7: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Negative,
			.value = true};
		
		case 27: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Negative,
			.value = false};
		
		// foreground	
		case 30: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Black)};
			
		case 31: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Red)};
		
		case 32: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Green)};
			
		case 33: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Yellow)};
			
		case 34: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Blue)};
			
		case 35: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Magenta)};

		case 36: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::Cyan)};

		case 37: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_ForegroundDefault};

		case 38: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground}; // extended - color set on caller site
			
		case 39: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = settings.colors.editorText}; // default
			
		// background
		case 40: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Black)};
			
		case 41: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Red)};
		
		case 42: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Green)};
			
		case 43: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Yellow)};
			
		case 44: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Blue)};
			
		case 45: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Magenta)};

		case 46: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::Cyan)};

		case 47: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_BackgroundDefault};

		case 48: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background}; // extended - color set on caller site
		
		// bright foreground
		case 90: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::DarkGray)};
			
		case 91: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::DarkSalmon)};
		
		case 92: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::LightGreen)};
			
		case 93: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::LightYellow)};
			
		case 94: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::LightBlue)};
			
		case 95: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::HotPink)};

		case 96: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::LightCyan)};

		case 97: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Foreground,
			.color = Color::FromKnown(D2D1::ColorF::LightGray)};	
		
		// bright background
		case 100: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::DarkGray)};
			
		case 101: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::DarkSalmon)};
		
		case 102: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::LightGreen)};
			
		case 103: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::LightYellow)};
			
		case 104: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::LightBlue)};
			
		case 105: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::HotPink)};

		case 106: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::LightCyan)};

		case 107: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Background,
			.color = Color::FromKnown(D2D1::ColorF::LightGray)};
		
		default: return ToolOutput::StyleChange {
			.type = ToolOutput::StyleChangeType_Unknown};
	}
}
