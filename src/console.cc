#include "console.hh"
#include "globals.hh"
#include "main-window.hh"
#include "key-bindings.hh"
#include "theme.hh"

#include "util/rect-util.hh"
#include "util/logging.hh"
#include "util/string-util.hh"
#include "util/diagnostics.hh"

#include "ui/constants.h"
#include "graphics/effects.hh"
#include "commands/tools.hh"

#include <charconv>
#include <algorithm>

//#################################################################################################
//
// Init + reset
//
//#################################################################################################

bool Console::Init() {
	scrollarea.barWidth = SCROLLBAR_WIDTH_WIDE;
	filePreview.Init();
	return true;
}

static void Reset(Console* self) {
	ASSERT(!self->process || !self->process->IsRunning());
	
	self->toolDiagnostics.clear();
	self->showToolDiagnostics = false;
	self->selectedDiagnosticsRecord = U64_MAX;
	
	self->progressRegex.Reset();
	self->progressValue = 0.0f;
	self->progressText.clear();
	
	self->diagnosticsRegex.Reset();
	self->diagnosticsRecords.clear();
	
	self->compiledCommandLine.clear();
	delete self->process;
	self->process = nullptr;
	
	self->styleChanges.clear();
	self->lines.clear();
	
	self->glyphRunCacheIsValid = false;
	self->glyphRunCache.clear();
	
	self->selectionStart = self->selectionEnd = TextPosition {};
}

//#################################################################################################
//
// Command compiling
//
//#################################################################################################

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
			builder->append(value.boolValue ? "true" : "false");
		} break;
		default: ASSERT_UNREACHABLE;
	};
}

static bool CompileCommand(Console* self) {
	if (self->toolParameterValues.size() < self->tool->parameters.size()) {
		self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
			.message = FormatString("Not enough parameters provided. Expected % but got %", self->toolParameterValues.size(), self->tool->parameters.size())});
		return false;
	}
		
	for (u64 pos, start = 0u; /**/; start = pos) {
		pos = self->tool->command.find('%', start);
		if (pos == std::string::npos) {
			self->compiledCommandLine.append(self->tool->command, start);
			break;
		}
		
		// append everything up to the percent
		self->compiledCommandLine.append(self->tool->command, start, (pos - start));
		start = pos+1;
		
		// check if it's an escaped percent - e.g. %%
		if (pos < self->tool->command.size()-1 && self->tool->command[pos+1] == '%') {
			self->compiledCommandLine.push_back('%');
			pos += 2;
			continue;
		}
		
		// check if it's reference to a parameter by index - e.g. %1
		if (pos < self->tool->command.size()-1 && IsNumeric(self->tool->command[pos+1])) {
			pos += 1;
			
			u64 parameterIndex = U64_MAX;
			const auto result = std::from_chars(self->tool->command.data()+pos, self->tool->command.data()+self->tool->command.size(), parameterIndex);
			
			// we just checked if its a numeric char so this should come back as ok
			ASSERT(result.ec == std::errc());
			ASSERT(parameterIndex < U64_MAX);
			
			if (parameterIndex >= self->toolParameterValues.size()) {
				self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
				 	.message  = FormatString("Parameter with index % not found (% parameters defined)", parameterIndex, self->toolParameterValues.size()),
				 	.source   = self->tool->command,
				 	.position = pos-1});
				return false;
			}
					
			const ParameterValue& paramValue = self->toolParameterValues[parameterIndex];
			const Parameter& paramDef = self->tool->parameters[parameterIndex];
			AppendParameterValue(&self->compiledCommandLine, paramValue, paramDef);
		
			pos = (result.ptr - self->tool->command.data());
			continue;
		}
		
		// check if it's a reference to a parameter by name - e.g. %(my_param)
		if (pos < self->tool->command.size()-1 && self->tool->command[pos+1] == '(') {
			pos += 2;
			
			const u64 posEnd = self->tool->command.find(')', pos);
			if (posEnd == std::string::npos) {
				self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
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
			
			self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
				.message  = FormatString("Parameter with name '%' not found", parameterName),
				.source   = self->tool->command,
				.position = pos-1});
			return false;
			
		found:
			AppendParameterValue(&self->compiledCommandLine, *paramValue, *paramDef);
			pos = posEnd+1;
		}
	}
	
	return true;
}

bool Console::StartProcess() {
	
	if (process && process->IsRunning()) {
		LogError("a process already running");
		return false;
	}
	
	Reset(this);
	
	// 
	// compile command
	//
	{
		if (!CompileCommand(this)) {
			showToolDiagnostics = true;
			isOpen = true;
			return false;
		}
	}
	
	//
	// compile regexes
	//
	{
		if (tool->HasProgress()) {
			if (!progressRegex.Compile(tool->progress.regex)) {
				toolDiagnostics.push_back(ToolDiagnosticsRecord {
					.message  = FormatString("'progress': %", progressRegex.GetErrorString()),
					.source   = tool->progress.regex,
					.position = U64_MAX});
			}
		}
		
		if (tool->HasDiagnosticsMatcher()) {
			if (!diagnosticsRegex.Compile(tool->diagnosticsMatcher.regex)) {
				toolDiagnostics.push_back(ToolDiagnosticsRecord {
					.message  = FormatString("'diagnostics': %", progressRegex.GetErrorString()),
					.source   = tool->diagnosticsMatcher.regex,
					.position = U64_MAX});
			}
		}
	}
	
	
	//
	// start process
	//
	{
		LogInfo("Running: %", compiledCommandLine);
		
		Process::StartInfo startInfo {
			.application = {},
			.commandLine = compiledCommandLine,
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
				.message = FormatString("Failed to start process. Last Error: %", FLastErr(GetLastError()))});
			isOpen = true;
			showToolDiagnostics = true;
			return false;
		}
	}
	
	return true;
}

//#################################################################################################
//
// Update
//
//#################################################################################################

static f32 GetToolbarHeight() {
	return MARGIN_X2 + theme.fontUi.lineHeight;
}

static void UpdateFilePreview(Console* self, const Console::EditorDiagnosticsRecord& record) {
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
	auto self = static_cast<Console*>(ud);
	
	ASSERT(self->process);
	self->process->Terminate();
}

static void OnRerunProcess(void* ud, u64) {}

static void OnClickToggleShowToolDiagnostics(void* ud, u64) {
	auto self = static_cast<Console*>(ud);
	self->showToolDiagnostics = !self->showToolDiagnostics;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void RenderOutput(Console* self) {
	
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
	foreground->Clear(theme.colors.editorText);

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
		foreground->CreateSolidColorBrush(theme.colors.editorText, &brushForeground);
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
				.top    = ln * theme.fontEditor.lineHeight,
				.right  = PADDING + to,
				.bottom = (ln+1) * theme.fontEditor.lineHeight};
		
			if (state.hasBackgroundColor)
				background->FillRectangle(rect, (state.hasNegative ? brushForeground : brushBackground));
				
			if (state.hasForegroundColor)
				foreground->FillRectangle(rect, (state.hasNegative ? brushBackground : brushForeground));
				
			if (state.hasUnderline)
				text->DrawLine(
					D2D_POINT_2F {.x = PADDING + rect.left,  .y = toolbarHeight + rect.top + theme.fontEditor.baselineOffset},
					D2D_POINT_2F {.x = PADDING + rect.right, .y = toolbarHeight + rect.top + theme.fontEditor.baselineOffset},
					alphaMaskBrush);
		};
		
		for (u64 i = 0u; i < self->styleChanges.size(); i++) {
			
			const Console::StyleChange* styleChange = &self->styleChanges[i];
			
			IterateTextRange(start, styleChange->position, ApplyStyle);
			
			// do the style change
			switch (styleChange->type) {
				case Console::StyleChangeType_Bold: break; // @TODO
				case Console::StyleChangeType_Underline: state.hasUnderline = styleChange->value; break;
				case Console::StyleChangeType_Negative: {
					state.hasNegative = styleChange->value;
				} break;
				case Console::StyleChangeType_Foreground: {
					brushForeground->SetColor(styleChange->color);
					state.hasForegroundColor = true;
				} break;
				case Console::StyleChangeType_ForegroundDefault: {
					state.hasForegroundColor = false;
				} break;
				case Console::StyleChangeType_Background: {
					brushBackground->SetColor(styleChange->color);
					state.hasBackgroundColor = true;
				} break;
				case Console::StyleChangeType_BackgroundDefault: {
					state.hasBackgroundColor = false;
				} break;
				case Console::StyleChangeType_Reset: {
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
			run.Draw(text, PADDING , (i * theme.fontEditor.lineHeight), theme.fontEditor, alphaMaskBrush);
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
			const Console::EditorDiagnosticsRecord& record = self->diagnosticsRecords[i];
			
			ASSERT(record.originLine < self->glyphRunCache.size());
			const GlyphRun& run = self->glyphRunCache[record.originLine];
			
			ASSERT(record.originFromColumn < record.originToColumn);
			
			f32 offsetFrom = .0f, offsetTo = .0f;
			run.MeasureOffsetRange(record.originFromColumn, record.originToColumn, &offsetFrom, &offsetTo);
			
			deviceContext->DrawRectangle(
				D2D_RECT_F {
					.left   = self->area.left + PADDING + offsetFrom,
					.top    = self->area.top  + toolbarHeight + ( record.originLine    * theme.fontEditor.lineHeight) - self->scrollarea.vpY,
					.right  = self->area.left + PADDING + offsetTo,
					.bottom = self->area.top  + toolbarHeight + ((record.originLine+1) * theme.fontEditor.lineHeight) - self->scrollarea.vpY},
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
					.top    = self->area.top  + toolbarHeight + (theme.fontEditor.lineHeight * ln)     - self->scrollarea.vpY,
					.right  = self->area.left + PADDING + offsetTo,
					.bottom = self->area.top  + toolbarHeight + (theme.fontEditor.lineHeight * (ln+1)) - self->scrollarea.vpY},
				theme.GetBrushSelection());
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
		
		if (mouse.Hittest(outputArea, self)) {
			const D2D_POINT_2F relativePoistion {mouse.x - outputArea.left, mouse.y - outputArea.top};
			
			const u64 hitLine = std::clamp<u64>(
				static_cast<u64>((relativePoistion.y + self->scrollarea.vpY) / theme.fontEditor.lineHeight),
				0u,
				self->glyphRunCache.size() - 1u);
			const GlyphRun& hitRun = self->glyphRunCache[hitLine];
			const u64 hitColumn    = hitRun.HitTest(relativePoistion.x);
			
			if (mouse.event == Mouse::Event_Down) {
				
				// check if we hit a matched diagnostic record
				for (u64 i = 0u; i < self->diagnosticsRecords.size(); i++) {
					const Console::EditorDiagnosticsRecord& record = self->diagnosticsRecords[i];
					
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
			} else if (mouse.IsDragging()) {
				self->selectionEnd = TextPosition {hitLine, hitColumn};
			}
		}
	}
	
	deviceContext->PopAxisAlignedClip();
	
	//
	// update file preview
	//
	if (self->selectedDiagnosticsRecord != U64_MAX) {
		const Console::EditorDiagnosticsRecord& record = self->diagnosticsRecords[self->selectedDiagnosticsRecord];
	
		self->filePreview.x = self->area.left - self->filePreview.width;
		self->filePreview.y = self->area.top  + toolbarHeight + ((record.originLine-2u) * theme.fontEditor.lineHeight) - self->scrollarea.vpY;
		self->filePreview.OnUpdate();
			
		deviceContext->DrawRectangle(self->filePreview.GetArea(), GetBrush(record.color));
	}
		
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void RenderToolDiagnostics(Console* self) {
	
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
		const Console::ToolDiagnosticsRecord& record = self->toolDiagnostics[i];
		
		run.Shape(record.message, theme.fontEditor);
		
		const D2D_POINT_2F position {
			.x = self->area.left + PADDING,
			.y = self->area.top + toolbarHeight + offsetTop - self->scrollarea.vpY};

		brush->SetColor(Diagnostics::SEVERITY_COLORS[self->process
			? Diagnostics::Severity_Error
			: Diagnostics::Severity_Warning]);

		run.Draw(deviceContext, position.x, position.y, theme.fontEditor, brush);
		
		if (!record.source.empty()) {
			run.Shape(record.source, theme.fontEditor);
			
			if (record.position < U64_MAX) {
				f32 from = .0f, to = .0f;
				run.MeasureOffsetRange(record.position, record.position+1, &from, &to);
				
				deviceContext->FillRectangle(
					D2D_RECT_F {
						.left   = position.x + from,
						.top    = position.y,
						.right  = position.x + to,
						.bottom = position.y + theme.fontEditor.lineHeight},
					brush);
			}
			
			run.Draw(deviceContext, position.x, position.y + theme.fontEditor.lineHeight, theme.fontEditor, theme.GetBrushEditorText());
			offsetTop += theme.fontEditor.lineHeight;
		}
		
		offsetTop += theme.fontEditor.lineHeight + PADDING;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Console::OnUpdate() {
		
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
		
		deviceContext->FillRectangle(toolbarArea, theme.GetBrushUiBackground());
	
		if (tool) {
			f32 offsetX = 0.0f;
						
			// draw tool name
			{
				run.Shape(tool->name, theme.fontUi);
				run.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, theme.fontUi, theme.GetBrushUiText());
				
				// underline
				deviceContext->DrawLine(
					D2D_POINT_2F {
						.x = toolbarArea.left + MARGIN,
						.y = toolbarArea.top  + MARGIN + theme.fontUi.underlineOffset},
					D2D_POINT_2F {
						.x = toolbarArea.left + MARGIN + run.width,
						.y = toolbarArea.top  + MARGIN + theme.fontUi.underlineOffset},
					theme.GetBrushUiText());
				
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
			if (progressRegex.IsOk()) {
				
				deviceContext->FillRoundedRectangle(
					MakeRoundedRect(progressArea.left, progressArea.top, (PROGRESS_AREA_WIDTH * progressValue), RectHeight(progressArea), RADIUS),
					GetBrush(D2D1::ColorF(D2D1::ColorF::Green)));
				
				deviceContext->DrawRoundedRectangle(
					MakeRoundedRect(progressArea, RADIUS),	
					theme.GetBrushUiText());
			} else {
				deviceContext->FillRoundedRectangle(
					MakeRoundedRect(progressArea, RADIUS),
					theme.GetBrushUiBackground(false));
			}
				
			// draw prgress text
			{
				std::string_view label = progressText;
				std::string_view hoverLabel;
				Mouse::OnClickFunction onClickFunc;
				D2D_COLOR_F labelColor;
				D2D_COLOR_F hoverLabelColor;
				
				char exitCodeBuffer[32] {'\0'};
				const u64 exitCode = process ? process->GetExitCode() : 0;
				
				if (!process) {
					onClickFunc = OnRerunProcess;
 					label = "Error";
 					hoverLabel = "Retry";
 					labelColor = D2D1::ColorF(D2D1::ColorF::Red);
 					hoverLabelColor = theme.colors.uiText;
				
				} else if (exitCode == STILL_ACTIVE) {
					onClickFunc = OnClickedKillProcess;
 					labelColor = theme.colors.uiText;
 					hoverLabel = "Terminate";
 					hoverLabelColor = D2D1::ColorF(D2D1::ColorF::Crimson);
				
				} else {
					onClickFunc = OnRerunProcess;
 					labelColor = (exitCode == 0)
	 					? theme.colors.uiText
	 					: D2D1::ColorF(D2D1::ColorF::Crimson);
 					hoverLabel = "Restart";
 					hoverLabelColor = theme.colors.uiText;
				}
				
				if (mouse.Hittest(progressArea, this, onClickFunc)) {
					deviceContext->FillRoundedRectangle(MakeRoundedRect(progressArea, RADIUS), theme.GetBrushHover(mouse.isDown));
					
					brush->SetColor(hoverLabelColor);
					run.Shape(hoverLabel, theme.fontUi);
				
				} else {
					brush->SetColor(labelColor);
					run.Shape(label, theme.fontUi);
				}
				
				const f32 x = area.left + offsetX + (PROGRESS_AREA_WIDTH / 2.0f) - (run.width / 2.0f);
				run.Draw(deviceContext, x, area.top + MARGIN, theme.fontUi, brush);
				
				offsetX += PROGRESS_AREA_WIDTH + MARGIN;
			}
			
			// draw error and warning icons
			if (!toolDiagnostics.empty()) {
				D2D_RECT_F areaBothIcons {
					.left  = toolbarArea.left + offsetX,
					.top   = toolbarArea.top + MARGIN - PADDING,
					.right = toolbarArea.left + offsetX + theme.fontUi.lineHeight + PADDING_X2,
					.bottom = toolbarArea.bottom - MARGIN + PADDING};
				
				if (showToolDiagnostics)
					deviceContext->FillRoundedRectangle(MakeRoundedRect(areaBothIcons, RADIUS), theme.GetBrushUiBackground(false));
					
				if (mouse.Hittest(areaBothIcons, this, OnClickToggleShowToolDiagnostics))
					deviceContext->FillRoundedRectangle(MakeRoundedRect(areaBothIcons, RADIUS), theme.GetBrushHover(mouse.isDown));
				
				deviceContext->DrawBitmap(
					theme.icons.editorDiagnosticsWarning,
					MakeRect(area.left + offsetX + PADDING, area.top + MARGIN, theme.fontUi.lineHeight, theme.fontUi.lineHeight));
				
				offsetX += theme.fontUi.lineHeight + PADDING_X2;
			}
			
		// no tool
		} else {
			run.Shape("No tool run yet.", theme.fontUi);
			run.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, theme.fontUi, theme.GetBrushUiText());
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
			run.Shape(line, theme.fontEditor);
		}
		
		glyphRunCacheIsValid = true;
	}
	
	//
	// update scrollarea (but don't render yet)
	//
	{
		f32 height = 0.0f;
		if (showToolDiagnostics) {
			height = toolDiagnostics.size() * theme.fontEditor.lineHeight;
			for (const ToolDiagnosticsRecord& rec : toolDiagnostics)
				if (rec.source.empty()) height += theme.fontEditor.lineHeight;
		} else {
			height = glyphRunCache.size() * theme.fontEditor.lineHeight;
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

void Console::OnResize(f32 newWidth, f32 newHeight) {
	area = D2D_RECT_F {
		.left = std::floor(mainWindow.width * 0.6f),
		.top = PADDING_X2 + theme.fontUi.lineHeight,
		.right = mainWindow.width,
		.bottom = mainWindow.height - PADDING_X2 - theme.fontUi.lineHeight};
	
	scrollarea.position = D2D_POINT_2F {
		.x = area.left,
		.y = area.top + GetToolbarHeight()};	
	scrollarea.vpSize = D2D_SIZE_F {
		.width = RectWidth(area),
		.height = RectHeight(area) - GetToolbarHeight()};
}

void Console::OnMouseWheel(f32 distance) {
	// @TODO(settings) scroll distance
	scrollarea.ScrollVertical(distance * theme.fontEditor.lineHeight * 5);
	disableAutoScroll = (scrollarea.vpY != scrollarea.GetMaxPositionY());	
}

static void ActionGotoDiagnostic(Console* self, u64 newSelectedRecord) {

}

bool Console::OnKeyDown(KeyEvent event) {
	if (event == keybinds.actions.gotoNextDiagnostic) {
		selectedDiagnosticsRecord = IncrementWrapAround(selectedDiagnosticsRecord, diagnosticsRecords.size());
		UpdateFilePreview(this, diagnosticsRecords[selectedDiagnosticsRecord]);
		return true;
		
	} else if (event == keybinds.actions.gotoPrevDiagnostic) {
		selectedDiagnosticsRecord = DecrementWrapAround(selectedDiagnosticsRecord, diagnosticsRecords.size());
		UpdateFilePreview(this, diagnosticsRecords[selectedDiagnosticsRecord]);
		return true;
	
	} else if (event == keybinds.actions.consoleTerminateProcess) {
		if (process) process->Terminate();
		return true;
	
	} else if (event == keybinds.actions.consoleCopy) {
		if (selectionStart == selectionEnd) return true;
		
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
	}
	
	return false;
}

//#################################################################################################
//
// Output processing
//
//#################################################################################################

static void WarnCaptureGroupOutOfRange(Console* self, const Regex::MatchResult& matchResult, u64 index, std::string_view groupName, std::string_view source) {
	self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
		.message  = FormatString("'%' % is out of range. Regex provides only % groups", groupName, index, matchResult.Size()),
		.source   = source,
		.position = U64_MAX});
}

static void WarnMatchedTextParseErr(Console* self, std::string_view groupName, std::string_view source, std::from_chars_result fcr) {
	self->toolDiagnostics.push_back(Console::ToolDiagnosticsRecord {
		.message  = FormatString("failed to parse matched text for '%': '%'", groupName, FFromCharsResult(fcr)),
		.source   = source,
		.position = U64_MAX});
}

static void MatchProgress(Console* self, const std::string* line) {
	if (!self->progressRegex.IsOk()) return;
	
	Regex::MatchResult matchResult {};
	if (!self->progressRegex.Match(*line, &matchResult)) return;
	if (self->tool->progress.captureGroupValue >= matchResult.Size()) {
		WarnCaptureGroupOutOfRange(self, matchResult, self->tool->progress.captureGroupValue, "capture-group-value", self->tool->progress.regex);
		self->progressRegex.Reset();
		return;
	}
	
	const Regex::MatchResult::Group& groupValue = matchResult.GetGroup(self->tool->progress.captureGroupValue);
	
	s64 newValue = 0;
	const std::from_chars_result fcrValue = std::from_chars(groupValue.begin, groupValue.end, newValue);
	if (fcrValue.ec != std::errc()) {
		WarnMatchedTextParseErr(self, "capture-group-value", groupValue.GetText(), fcrValue);
		return;
	}
	
	s64 newMax = self->tool->progress.maxValue;
	if (self->tool->progress.captureGroupMax < matchResult.Size()) {
		const Regex::MatchResult::Group& groupMax = matchResult.GetGroup(self->tool->progress.captureGroupMax);
		const std::from_chars_result fcrMax = std::from_chars(groupMax.begin, groupMax.end, newMax);
		if (fcrMax.ec != std::errc()) {
			WarnMatchedTextParseErr(self, "capture-group-max", groupMax.GetText(), fcrMax);
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
			LogWarning("converting to precentage text failed. Error: %", FToCharsResult(tcr));
			self->progressText.assign("ERROR");
		}
	
	} else if (self->tool->progress.format == Tool::Progress::Format_Absolute) {
		self->progressText.assign(groupValue.GetText());
		self->progressText.append(" / ");
		
		if (self->tool->progress.captureGroupMax < matchResult.Size()) {
			self->progressText.append(matchResult.GetGroup(self->tool->progress.captureGroupMax).GetText());
		
		} else {
			constexpr u64 bufferSize = 16;
			char buffer[bufferSize] {'\0'};
			
			const std::to_chars_result resultToCh = std::to_chars(buffer, buffer+bufferSize, self->tool->progress.maxValue);
			if (resultToCh.ec == std::errc()) {
				self->progressText.append(buffer, resultToCh.ptr);
			
			} else {
				LogWarning("converting to max text failed. Error: %", FToCharsResult(resultToCh));
				self->progressText.append("ERROR");
			}
		}
	
	} else {
		ASSERT_UNREACHABLE
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void MatchDiagnostics(Console* self, const std::string* line) {
 	if (!self->progressRegex.IsOk()) return;
	 
	Regex::MatchResult matchResult {};
	if (!self->diagnosticsRegex.Match(*line, &matchResult)) return;

	Console::EditorDiagnosticsRecord record {};
	
	record.color = theme.colors.editorText;
	record.originLine = self->lines.size() - 1u;
	record.originFromColumn = matchResult.groups.front().begin - line->data();
	record.originToColumn = matchResult.groups.front().end - line->data();
	
	// color
	if (self->tool->diagnosticsMatcher.captureGroupColor < matchResult.Size()) {
		const Regex::MatchResult::Group& group = matchResult.GetGroup(self->tool->diagnosticsMatcher.captureGroupColor);
		
		for (const Tool::DiagnosticsMatcher::ColorMapping& entry : self->tool->diagnosticsMatcher.colorMapping) {
			if (StringEqualsCasesInsen(entry.key, group.GetText())) {
				record.color = entry.color;
				break;
			}
		}
	}
	
	// file
	if (self->tool->diagnosticsMatcher.captureGroupFile < matchResult.Size()) {
		const Regex::MatchResult::Group& group = matchResult.GetGroup(self->tool->diagnosticsMatcher.captureGroupFile);
		record.file = group.GetText();
	}
	
	// line
	if (self->tool->diagnosticsMatcher.captureGroupLine < matchResult.Size()) {
		const Regex::MatchResult::Group& group = matchResult.GetGroup(self->tool->diagnosticsMatcher.captureGroupLine);
		const std::from_chars_result fcr = std::from_chars(group.begin, group.end, record.line);
		
		if (fcr.ec != std::errc()) {
			WarnMatchedTextParseErr(self, "capture-group-line", group.GetText(), fcr);
			record.line = 0u;
		}
		
		if (self->tool->diagnosticsMatcher.linesStartAtOne && record.line > 0u)
			record.line -= 1;
	}
	
	self->diagnosticsRecords.push_back(std::move(record));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static Console::StyleChange GetStyleChange(int value);

static void ParseChunk(Console* self, std::string_view data) {

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
				
				Console::StyleChange styleChange = GetStyleChange(value);
				if (styleChange.type == Console::StyleChangeType_Unknown) continue;
				
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
					
					styleChange.color = D2D_COLOR_F {r/255.0f, g/255.0f, b/255.0f, 1.0f};
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


void Console::OnStderr(std::string_view data) {
	ParseChunk(this, data);
	mainWindow.PostUpdate();
}

void Console::OnStdout(std::string_view data) {
	ParseChunk(this, data);
	mainWindow.PostUpdate();
}

void Console::OnStarted() {
	std::scoped_lock lock {mtx};
	
	LogInfo("tool '%' started", tool->name);
	
	lines.emplace_back();
	progressText = (tool->progress.format == Tool::Progress::Format_Percent)
		? "0%"
		: "Running";
	
	if (tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnStart)
		isOpen = true;
		
	mainWindow.PostUpdate();
}

void Console::OnExited(int exitCode) {	
	std::scoped_lock lock {mtx};
	
	LogInfo("tool '%' exited with code %", tool->name, exitCode);
	
	char buffer[32] {'\0'};
	const u64 bufferLen = FormatToBuffer(buffer, "Exit %", exitCode);
	
	progressText.assign(buffer, bufferLen);
	
	const u32 flagToTest = exitCode == 0
		? tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitSuccess
		: tool->consoleOpenFlags & Tool::ConsoleOpenFlags_OnExitError;
	
	if (tool->consoleOpenFlags & flagToTest)
		isOpen = true;
		
	mainWindow.PostUpdate();
}

Console::StyleChange GetStyleChange(int value) {
	switch (value) {
		case 0: return Console::StyleChange {
			.type = Console::StyleChangeType_Reset};
		
		// boldness	
		case 1: return Console::StyleChange {
			.type = Console::StyleChangeType_Bold,
			.value = true};
			
		case 22: return Console::StyleChange {
			.type = Console::StyleChangeType_Bold,
			.value = false};
		
		// underline	
		case 4: return Console::StyleChange {
			.type = Console::StyleChangeType_Underline,
			.value = true};
			 
		case 24: return Console::StyleChange {
			.type = Console::StyleChangeType_Underline,
			.value = false};
		
		// negative	
		case 7: return Console::StyleChange {
			.type = Console::StyleChangeType_Negative,
			.value = true};
		
		case 27: return Console::StyleChange {
			.type = Console::StyleChangeType_Negative,
			.value = false};
		
		// foreground	
		case 30: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Black)};
			
		case 31: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Red)};
		
		case 32: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Green)};
			
		case 33: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Yellow)};
			
		case 34: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Blue)};
			
		case 35: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Magenta)};

		case 36: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::Cyan)};

		case 37: return Console::StyleChange {
			.type = Console::StyleChangeType_ForegroundDefault};

		case 38: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground}; // extended - color set on caller site
			
		case 39: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = theme.colors.editorText}; // default
			
		// background
		case 40: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Black)};
			
		case 41: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Red)};
		
		case 42: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Green)};
			
		case 43: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Yellow)};
			
		case 44: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Blue)};
			
		case 45: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Magenta)};

		case 46: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::Cyan)};

		case 47: return Console::StyleChange {
			.type = Console::StyleChangeType_BackgroundDefault};

		case 48: return Console::StyleChange {
			.type = Console::StyleChangeType_Background}; // extended - color set on caller site
		
		// bright foreground
		case 90: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::DarkGray)};
			
		case 91: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::DarkSalmon)};
		
		case 92: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::LightGreen)};
			
		case 93: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::LightYellow)};
			
		case 94: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::LightBlue)};
			
		case 95: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::HotPink)};

		case 96: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::LightCyan)};

		case 97: return Console::StyleChange {
			.type = Console::StyleChangeType_Foreground,
			.color = D2D1::ColorF(D2D1::ColorF::LightGray)};	
		
		// bright background
		case 100: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::DarkGray)};
			
		case 101: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::DarkSalmon)};
		
		case 102: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::LightGreen)};
			
		case 103: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::LightYellow)};
			
		case 104: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::LightBlue)};
			
		case 105: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::HotPink)};

		case 106: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::LightCyan)};

		case 107: return Console::StyleChange {
			.type = Console::StyleChangeType_Background,
			.color = D2D1::ColorF(D2D1::ColorF::LightGray)};
		
		default: return Console::StyleChange {
			.type = Console::StyleChangeType_Unknown};
	}
}
