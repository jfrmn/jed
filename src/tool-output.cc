#include "tool-output.hh"
#include "globals.hh"
#include "settings.hh"
#include "util.hh"
#include "logging.hh"
#include "tools.hh"

#include "util/diagnostics.hh"
#include "ui/constants.h"
#include "ui/window.hh"
#include "graphics/effects.hh"

#include <charconv>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Init + reset
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool ToolOutput::Init() {
	scrollarea.barWidth = SCROLLBAR_WIDTH_WIDE;
	filePreview.Init();
	return true;
}

static void Reset(ToolOutput* self) {
	ASSERT(!self->process || !self->process->IsRunning());
	
	self->toolDiagnostics.clear();
	self->showToolDiagnostics = false;
	self->selectedDiagnosticsRecord = U64_MAX;
	
	self->progressValue = 0.0f;
	self->progressText.clear();
	
	self->diagnosticsRecords.clear();
	
	delete self->process;
	self->process = nullptr;
	
	self->styleChanges.clear();
	self->lines.clear();
	
	self->glyphRunCacheIsValid = false;
	self->glyphRunCache.clear();
	
	self->selectionStart = self->selectionEnd = TextPosition {};
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
			.message = FormatString("Not enough parameters provided. Expected %u but got %u", self->toolParameterValues.size(), self->tool->parameters.size())});
		return false;
	}
			
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
			
			if (parameterIndex >= self->toolParameterValues.size()) {
				self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
				 	.message  = FormatString("Parameter with index %u not found (%u parameters defined)", parameterIndex, self->toolParameterValues.size()),
				 	.source   = self->tool->command,
				 	.position = pos-1});
				return false;
			}
					
			const ParameterValue& paramValue = self->toolParameterValues[parameterIndex];
			const Parameter& paramDef = self->tool->parameters[parameterIndex];
			AppendParameterValue(commandLine, paramValue, paramDef);
		
			pos = (fcr.ptr - self->tool->command.data());
			continue;
		}
		
		// check if it's a reference to a parameter by name - e.g. %(my_param)
		if (pos < self->tool->command.size()-1 && self->tool->command[pos+1] == '(') {
			pos += 2;
			
			const u64 posEnd = self->tool->command.find(')', pos);
			if (posEnd == std::string::npos) {
				self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
					.message  = "Missing closing ')' for parameter-reference by name",
					.source   = self->tool->command,
					.position = pos-1});
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
				.message  = FormatString("Parameter with name '%.*s' not found", SIZE_AND_DATA(parameterName)),
				.source   = self->tool->command,
				.position = pos-1});
			return false;
			
		found:
			AppendParameterValue(commandLine, *paramValue, *paramDef);
			pos = posEnd+1;
		}
	}
	
	return true;
}

bool ToolOutput::StartProcess() {
	
	if (process && process->IsRunning()) {
		LogError("a process already running");
		return false;
	}
	
	Reset(this);
	
	//
	// start process
	//
	{
		std::string commandLine {};
		if (!CompileCommand(this, &commandLine)) {
			showToolDiagnostics = true;
			isOpen = true;
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
				.message = FormatString("Failed to start process. Last Error: %s", StrLastErr(GetLastError()))});
			isOpen = true;
			showToolDiagnostics = true;
			return false;
		}
	}
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Update
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static f32 GetToolbarHeight() {
	return MARGIN_X2 + settings.fontUi.lineHeight;
}

static void UpdateFilePreview(ToolOutput* self, const ToolOutput::EditorDiagnosticsRecord& record) {
	self->selectionStart = TextPosition {record.originLine, record.originFromColumn};
	self->selectionEnd   = TextPosition {record.originLine, record.originToColumn};
	self->filePreview.Load(FilePreview::LoadArgs {
		.path = record.file,
		.mode = FilePreview::LoadMode_TargetLine,
		.targetLine = record.line});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnOpenConfigurator(void* ud, u64) {}

static void OnClickedKillProcess(void* ud, u64) {
	auto self = static_cast<ToolOutput*>(ud);
	
	ASSERT(self->process);
	self->process->Terminate();
}

static void OnRerunProcess(void* ud, u64) {}

static void OnClickToggleShowToolDiagnostics(void* ud, u64) {
	auto self = static_cast<ToolOutput*>(ud);
	self->showToolDiagnostics = !self->showToolDiagnostics;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void RenderOutput(ToolOutput* self) {
	
	const f32 toolbarHeight = GetToolbarHeight();
	
	//
	// prepare offscreen render targets
	//
	const D2D_SIZE_F areaSize {
		.width = RectWidth(self->area),
		.height = RectHeight(self->area) - toolbarHeight};
	
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
	if (!self->styleChanges.empty()) {
	
		background->SetTransform(D2D1::Matrix3x2F::Translation(self->area.left, self->area.top + toolbarHeight));
		DEFER(background->SetTransform(D2D1::Matrix3x2F::Identity()));
		
		foreground->SetTransform(D2D1::Matrix3x2F::Translation(self->scrollarea.vpX, -self->scrollarea.vpY));
		
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
			const GlyphRun& run = self->glyphRunCache[ln];
								
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
		
		for (u64 i = 0u; i < self->styleChanges.size(); i++) {
			
			const ToolOutput::StyleChange* styleChange = &self->styleChanges[i];
			
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
		text->SetTransform(D2D1::Matrix3x2F::Translation(self->scrollarea.vpX, -self->scrollarea.vpY));
		
		for (u64 i = 0; i < self->glyphRunCache.size(); i++) {
			const GlyphRun& run = self->glyphRunCache[i];
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
		BlendImages(deviceContext, {self->area.left, self->area.top + toolbarHeight}, bmForeground, bmText);
		
		bmForeground->Release();
		bmText->Release();
	}
	
	deviceContext->PushAxisAlignedClip(
		D2D_RECT_F {
			.left = self->area.left,
			.top = self->area.top + toolbarHeight,
			.right = self->area.right,
			.bottom = self->area.bottom},
		D2D1_ANTIALIAS_MODE_ALIASED);
	
	//
	// draw matched diagnostics
	//
	{		
		for (u64 i = 0; i < self->diagnosticsRecords.size(); i++) {
			const ToolOutput::EditorDiagnosticsRecord& record = self->diagnosticsRecords[i];
			
			ASSERT(record.originLine < self->glyphRunCache.size());
			const GlyphRun& run = self->glyphRunCache[record.originLine];
			
			ASSERT(record.originFromColumn < record.originToColumn);
			
			f32 offsetFrom = .0f, offsetTo = .0f;
			run.MeasureOffsetRange(record.originFromColumn, record.originToColumn, &offsetFrom, &offsetTo);
			
			deviceContext->DrawRectangle(
				D2D_RECT_F {
					.left   = self->area.left + PADDING + offsetFrom,
					.top    = self->area.top  + toolbarHeight + ( record.originLine    * settings.fontEditor.lineHeight) - self->scrollarea.vpY,
					.right  = self->area.left + PADDING + offsetTo,
					.bottom = self->area.top  + toolbarHeight + ((record.originLine+1) * settings.fontEditor.lineHeight) - self->scrollarea.vpY},
				GetBrush(record.color));
		}
	}
	
	//
	// draw selection
	//
	if (self->selectionStart != self->selectionEnd) {
		
		const TextPosition* from = nullptr, *to  = nullptr;
		if (self->selectionStart < self->selectionEnd) from = &self->selectionStart, to = &self->selectionEnd;
		else from = &self->selectionEnd, to = &self->selectionStart;
		
		IterateTextRange(*from, *to, [self, toolbarHeight](u64 ln, u64 fromCp, u64 toCp) {
			const GlyphRun& run = self->glyphRunCache[ln];
								
			f32 offsetFrom = .0f, offsetTo = .0f;
			run.MeasureOffsetRange(fromCp, toCp, &offsetFrom, &offsetTo);
			
			deviceContext->FillRectangle(
				D2D_RECT_F {
					.left   = self->area.left + PADDING + offsetFrom,
					.top    = self->area.top  + toolbarHeight + (settings.fontEditor.lineHeight * ln)     - self->scrollarea.vpY,
					.right  = self->area.left + PADDING + offsetTo,
					.bottom = self->area.top  + toolbarHeight + (settings.fontEditor.lineHeight * (ln+1)) - self->scrollarea.vpY},
				settings.GetBrushSelection());
		});
	}
	
	//
	// hittest to change selection
	//
	{
		const D2D_RECT_F outputArea {
			.left = self->area.left,
			.top = self->area.top + toolbarHeight,
			.right = self->area.right,
			.bottom = self->area.bottom};
		
		if (mouse.Hittest(outputArea, self, nullptr)) {
			const D2D_POINT_2F relativePoistion {mouse.x - outputArea.left, mouse.y - outputArea.top};
			
			const u64 hitLine = std::clamp<u64>(
				static_cast<u64>((relativePoistion.y + self->scrollarea.vpY) / settings.fontEditor.lineHeight),
				0u,
				self->glyphRunCache.size() - 1u);
			const GlyphRun& hitRun = self->glyphRunCache[hitLine];
			const u64 hitColumn    = hitRun.HitTest(relativePoistion.x);
			
			if (mainWindow.event.type == Event::Type_MouseDown) {				
				
				// check if we hit a matched diagnostic record
				for (u64 i = 0u; i < self->diagnosticsRecords.size(); i++) {
					const ToolOutput::EditorDiagnosticsRecord& record = self->diagnosticsRecords[i];
					
					const bool hitThisRecord = record.originLine == hitLine &&
				                           	record.originFromColumn <= hitColumn &&
				                           	record.originToColumn >= hitColumn;
					if (hitThisRecord) {
						UpdateFilePreview(self, record);
						self->selectedDiagnosticsRecord = i;
						goto hit_record;
					}
				}
			
				mouse.StartDragging();
				self->selectionStart = self->selectionEnd = TextPosition {hitLine, hitColumn};
				self->selectedDiagnosticsRecord = U64_MAX;
				
			hit_record: __noop;
			} else if (mouse.isDragging) {
				self->selectionEnd = TextPosition {hitLine, hitColumn};
			}
		}
	}
	
	deviceContext->PopAxisAlignedClip();
	
	//
	// update file preview
	//
	if (self->selectedDiagnosticsRecord != U64_MAX) {
		const ToolOutput::EditorDiagnosticsRecord& record = self->diagnosticsRecords[self->selectedDiagnosticsRecord];
	
		self->filePreview.x = self->area.left - self->filePreview.width;
		self->filePreview.y = self->area.top  + toolbarHeight + ((record.originLine-2u) * settings.fontEditor.lineHeight) - self->scrollarea.vpY;
		self->filePreview.OnUpdate();
			
		deviceContext->DrawRectangle(self->filePreview.GetArea(), GetBrush(record.color));
	}
		
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void RenderToolDiagnostics(ToolOutput* self) {
	
	const f32 toolbarHeight = GetToolbarHeight();	
	
	deviceContext->PushAxisAlignedClip(
	D2D_RECT_F {
		.left = self->area.left,
		.top = self->area.top + toolbarHeight,
		.right = self->area.right,
		.bottom = self->area.bottom},
	D2D1_ANTIALIAS_MODE_ALIASED);
	DEFER(deviceContext->PopAxisAlignedClip());
	
	GlyphRun run;
	f32 offsetTop = 0.0f;
	for (u64 i = 0; i < self->toolDiagnostics.size(); i++) {
		const ToolOutput::ToolDiagnosticsRecord& record = self->toolDiagnostics[i];
		
		run.Shape(record.message, settings.fontEditor);
		
		const D2D_POINT_2F position {
			.x = self->area.left + PADDING,
			.y = self->area.top + toolbarHeight + offsetTop - self->scrollarea.vpY};

		brush->SetColor(Diagnostics::SEVERITY_COLORS[self->process
			? Diagnostics::Severity_Error
			: Diagnostics::Severity_Warning].ToD2D());

		run.Draw(deviceContext, position.x, position.y, settings.fontEditor, brush);
		
		if (!record.source.empty()) {
			run.Shape(record.source, settings.fontEditor);
			
			if (record.position < U64_MAX) {
				f32 from = .0f, to = .0f;
				run.MeasureOffsetRange(record.position, record.position+1, &from, &to);
				
				deviceContext->FillRectangle(
					D2D_RECT_F {
						.left   = position.x + from,
						.top    = position.y,
						.right  = position.x + to,
						.bottom = position.y + settings.fontEditor.lineHeight},
					brush);
			}
			
			run.Draw(deviceContext, position.x, position.y + settings.fontEditor.lineHeight, settings.fontEditor, settings.GetBrushEditorText());
			offsetTop += settings.fontEditor.lineHeight;
		}
		
		offsetTop += settings.fontEditor.lineHeight + PADDING;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ToolOutput::Update() {
		
	//
	// draw backgound
	//
	{
		ID2D1Bitmap* bitmap = CopyFromRenderTarget(deviceContext, area);
		if (!bitmap) return;
		DrawGlow(deviceContext, bitmap, area);
		
		PushLayer(deviceContext, area);	
		BlurArea(deviceContext, area, bitmap);
		
		bitmap->Release();
		PopLayer(deviceContext);
	}
	
	const std::scoped_lock lock {mtx};
	
	GlyphRun run = {};
	f32 offsetTop = 0.0f;
	
	//
	// draw toolbar
	//
	{
		const D2D_RECT_F toolbarArea {
			.left = area.left,
			.top = area.top,
			.right = area.right,
			.bottom = area.top + GetToolbarHeight()};
		
		deviceContext->FillRectangle(toolbarArea, settings.GetBrushUiBackground());
	
		if (tool) {
			f32 offsetX = 0.0f;
						
			// draw tool name
			{
				run.Shape(tool->name, settings.fontUi);
				run.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, settings.fontUi, settings.GetBrushUiText());
				
				// underline
				deviceContext->DrawLine(
					D2D_POINT_2F {
						.x = toolbarArea.left + MARGIN,
						.y = toolbarArea.top  + MARGIN + settings.fontUi.underlineOffset},
					D2D_POINT_2F {
						.x = toolbarArea.left + MARGIN + run.width,
						.y = toolbarArea.top  + MARGIN + settings.fontUi.underlineOffset},
					settings.GetBrushUiText());
				
				offsetX = run.width + MARGIN_X2;
				
				const D2D_RECT_F toolNameArea {
					.left = toolbarArea.left,
					.top = toolbarArea.top,
					.right = toolbarArea.left + offsetX,
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
					GetBrush(Color::FromKnown(D2D1::ColorF::Green)));
				
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
					run.Shape(hoverLabel, settings.fontUi);
				
				} else {
					brush->SetColor(labelColor.ToD2D());
					run.Shape(label, settings.fontUi);
				}
				
				const f32 x = area.left + offsetX + (PROGRESS_AREA_WIDTH / 2.0f) - (run.width / 2.0f);
				run.Draw(deviceContext, x, area.top + MARGIN, settings.fontUi, brush);
				
				offsetX += PROGRESS_AREA_WIDTH + MARGIN;
			}
			
			// draw error and warning icons
			if (!toolDiagnostics.empty()) {
				D2D_RECT_F areaBothIcons {
					.left  = toolbarArea.left + offsetX,
					.top   = toolbarArea.top + MARGIN - PADDING,
					.right = toolbarArea.left + offsetX + settings.fontUi.lineHeight + PADDING_X2,
					.bottom = toolbarArea.bottom - MARGIN + PADDING};
				
				if (showToolDiagnostics)
					deviceContext->FillRoundedRectangle(ToRounded(areaBothIcons), settings.GetBrushUiBackground(false));
					
				if (mouse.Hittest(areaBothIcons, this, OnClickToggleShowToolDiagnostics))
					deviceContext->FillRoundedRectangle(ToRounded(areaBothIcons), settings.GetBrushHover(mouse.isDown));
				
				deviceContext->DrawBitmap(
					settings.icons.editorDiagnosticsWarning,
					MakeRect(area.left + offsetX + PADDING, area.top + MARGIN, settings.fontUi.lineHeight, settings.fontUi.lineHeight));
				
				offsetX += settings.fontUi.lineHeight + PADDING_X2;
			}
			
		// no tool
		} else {
			run.Shape("No tool run yet.", settings.fontUi);
			run.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, settings.fontUi, settings.GetBrushUiText());
		}
		
		offsetTop += GetToolbarHeight();
	}
	
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
		f32 height = 0.0f;
		if (showToolDiagnostics) {
			height = toolDiagnostics.size() * settings.fontEditor.lineHeight;
			for (const ToolDiagnosticsRecord& rec : toolDiagnostics)
				if (rec.source.empty()) height += settings.fontEditor.lineHeight;
		} else {
			height = glyphRunCache.size() * settings.fontEditor.lineHeight;
		}
		
		scrollarea.totalSize = D2D_SIZE_F {
			.width  = RectWidth(area),
			.height = height};
		if (!disableAutoScroll)
			scrollarea.vpY = scrollarea.GetMaxPositionY();
	}
	
	//
	// render lines
	//
	if (showToolDiagnostics)
		RenderToolDiagnostics(this);
	else
		RenderOutput(this);
	
	//
	// scrollarea
	//
	{
		scrollarea.OnUpdate();
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
		.y = area.top + GetToolbarHeight()};	
	scrollarea.vpSize = D2D_SIZE_F {
		.width = RectWidth(area),
		.height = RectHeight(area) - GetToolbarHeight()};
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

static void WarnMatchedTextParseErr(ToolOutput* self, std::string_view groupName, std::string_view text, std::from_chars_result fcr) {
	self->toolDiagnostics.push_back(ToolOutput::ToolDiagnosticsRecord {
		.message  = FormatString("failed to parse matched text for capture group '%.*s': '%s'", SIZE_AND_DATA(groupName), Str(fcr)),
		.source   = text,
		.position = U64_MAX});
}

static void MatchProgress(ToolOutput* self, const std::string* line) {
	if (!self->tool->progress.regex.isOk) return;
	
	RegexMatch match;
	if (!self->tool->progress.regex.Match(*line, &match)) return;
	if (self->tool->progress.captureGroupValue >= match.groupCount) {
		LogError("capture group 'value' (index %u) is out of range. Regex only provided only %u capture groups", self->tool->progress.captureGroupValue, self->tool->progress.regex.captureGroupCount);
		return;
	}
	
	const RegexMatch::Group groupValue = match.GetGroup(self->tool->progress.captureGroupValue);
	
	s64 newValue = 0;
	const std::from_chars_result fcrValue = std::from_chars(groupValue.begin, groupValue.end, newValue);
	if (fcrValue.ec != std::errc()) {
		WarnMatchedTextParseErr(self, "group-value", groupValue.GetText(), fcrValue);
		return;
	}
	
	s64 newMax = self->tool->progress.maxValue;
	if (self->tool->progress.captureGroupMax < match.groupCount) {
		
		const RegexMatch::Group groupMax = match.GetGroup(self->tool->progress.captureGroupMax);
		
		const std::from_chars_result fcrMax = std::from_chars(groupMax.begin, groupMax.end, newMax);
		if (fcrMax.ec != std::errc()) {
			WarnMatchedTextParseErr(self, "group-max", groupMax.GetText(), fcrMax);
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
		ASSERT_UNREACHABLE
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void MatchDiagnostics(ToolOutput* self, const std::string* line) {
 	if (!self->tool->diagnostics.regex.isOk) return;
	 
	RegexMatch match {};
	if (!self->tool->diagnostics.regex.Match(*line, &match)) return;

	ToolOutput::EditorDiagnosticsRecord record {};
	record.color = settings.colors.editorText;
	record.originLine = self->lines.size() - 1u;
	
	const RegexMatch::Group fullMatch = match.GetFullMatch();
	record.originFromColumn = fullMatch.begin - line->data();
	record.originToColumn = fullMatch.end - line->data();
	
	// color
	if (self->tool->diagnostics.captureGroupColor < match.groupCount) {
		const RegexMatch::Group& group = match.GetGroup(self->tool->diagnostics.captureGroupColor);
		
		for (const Tool::DiagnosticsMatcher::ColorMapping& entry : self->tool->diagnostics.colorMapping) {
			if (StringEqualsCaseInsen(entry.key, group.GetText())) {
				record.color = entry.color;
				break;
			}
		}
	}
	
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
			WarnMatchedTextParseErr(self, "group-line", group.GetText(), fcr);
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
					.column = line->size()};
				
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
	
	if (tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnStart)
		isOpen = true;
		
	mainWindow.PostUpdate();
}

void ToolOutput::OnExited(int exitCode) {	
	std::scoped_lock lock {mtx};
	
	LogInfo("tool '%.*s' exited with code %d", SIZE_AND_DATA(tool->name), exitCode);
	
	FormatString(&progressText, "Exit %d", exitCode);
	
	const u32 flagToTest = exitCode == 0
		? tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitSuccess
		: tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitError;
	
	if (tool->consoleOpenFlags & flagToTest)
		isOpen = true;
		
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
