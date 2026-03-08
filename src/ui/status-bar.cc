#include "status-bar.hh"
#include "globals.hh"
#include "main-window.hh"

#include "editor/editor.hh"
#include "editor/editor-diagnostics.hh"
#include "editor/editor-diagnosticslist.hh"

#include "ui/constants.h"
#include "ui/style.hh"

#include "util/rect-util.hh"
#include "language/language.hh"
#include "graphics/effects.hh"
#include "commands/tools.hh"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
constexpr D2D_COLOR_F languageServerStatusColors[] {
	D2D_COLOR_F {0.5f, 0.5f, 0.5f, 1.0f}, // standby
	D2D_COLOR_F {0.0f, 0.5f, 0.0f, 1.0f}, // initializing
	D2D_COLOR_F {0.0f, 0.5f, 0.0f, 1.0f}, // running
	D2D_COLOR_F {0.5f, 0.5f, 0.0f, 1.0f}, // shutting down
	D2D_COLOR_F {1.0f, 1.0f, 0.0f, 1.0f}, // exited
	D2D_COLOR_F {1.0f, 0.0f, 0.0f, 1.0f}  // crashed
};

static const char* LAYER = "StatusBar";
static const f32 LANGUAGE_STATUS_INDICATOR_WIDTH = 5.0f;

//#################################################################################################
// 
// H E L P E R
//
//#################################################################################################

static bool IsL2R(const StatusBar* statusBar, u64 i) {
	return i < statusBar->l2rElementCount;
}

static D2D_RECT_F GetArea(const f32 posX, const f32 width, bool l2r) {
	
	f32 left = .0f, right = .0f;
	if (l2r) {
		left  = posX;
		right = posX + width;
	} else {
		left  = posX - width;
		right = posX;
	}
	
	return D2D_RECT_F {
		.left   = left,
		.top    = mainWindow.height - PADDING_X2 - style.fontUi.lineHeight,
		.right  = right,
		.bottom = mainWindow.height};
}

static Editor* GetFocusedEditor() {
	return mainWindow.panels[mainWindow.focusedPanelIndex].editor;
}

//#################################################################################################
// 
// S T A T U S   B A R   E L E M E N T S
//
//#################################################################################################

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// None

static f32 UpdateNone(StatusBar* self, f32 posX, u64 i) {
	return posX;
}

static bool IsVisibleTrue() {
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Padding

static f32 UpdatePadding(StatusBar* self, f32 posX, u64 i) {
	const f32 amount = 200.0f;
	return IsL2R(self, i)
		? posX + amount
		: posX - amount;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Explorer Button

static void OnClickExplorerItem(void*, u64) {
	// @TODO
}

static f32 UpdateExplorerButton(StatusBar* self, f32 posX, u64 i) {
	
	const bool l2r = IsL2R(self, i);	
	const f32 width = PADDING_X2 + style.fontUi.lineHeight;
	const D2D_RECT_F area = GetArea(posX, width, l2r);
		
	Style::Icon icon = mainWindow.explorer ? Style::Icon_Explorer_FolderOpen : Style::Icon_Explorer_FolderClosed;
	deviceContext->DrawBitmap(style.icons[icon], D2D_RECT_F {
		.left   = area.left + PADDING,
		.top    = mainWindow.height - style.fontUi.lineHeight - PADDING,
		.right  = area.right - PADDING,
		.bottom = mainWindow.height - PADDING});
	
	if (mouse.Hittest(area, self, OnClickExplorerItem)) {
		deviceContext->FillRectangle(area, style.GetBrushHover(mouse.isDown));
	}
	
	return l2r ? area.right : area.left;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Console Button

// @TODO
static f32 UpdateConsoleButton(StatusBar* self, f32 posX, u64 i) {
	return posX;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Console Progress

static bool IsVisibleConsoleProgress() {
	if (!mainWindow.console.tool) return false;
	if (mainWindow.console.tool->progress.hideFromStatusBar) return false;
	if (!mainWindow.console.progressRegex.IsOk()) return false;
	return true;
}

static f32 UpdateConsoleProgress(StatusBar* self, f32 posX, u64 i) {
	if (!mainWindow.console.tool) return posX;
	if (mainWindow.console.tool->progress.hideFromStatusBar) return posX;
	if (!mainWindow.console.progressRegex.IsOk()) return posX;
	
	constexpr f32 PROGRESS_BAR_WIDTH = 100.0f;

	const bool l2r = IsL2R(self, i);	
	const D2D_RECT_F area = GetArea(posX, PROGRESS_BAR_WIDTH, l2r);
	
	const std::scoped_lock lock {mainWindow.console.mtx};
	
	style.brush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
	deviceContext->FillRoundedRectangle(
		MakeRoundedRect(
			area.left,
			area.top + PADDING,
			PROGRESS_BAR_WIDTH * mainWindow.console.progressValue,
			style.fontUi.lineHeight,
			RADIUS),
		style.brush);
	
	deviceContext->DrawRoundedRectangle(
		MakeRoundedRect(
			area.left,
			area.top + PADDING,
			PROGRESS_BAR_WIDTH,
			style.fontUi.lineHeight,
			RADIUS),
		style.GetBrushUiText());
	
	GlyphRun run;
	run.Shape(mainWindow.console.progressText, style.fontUi);
	run.DrawCenter(deviceContext,
		area.left,
		area.top + PADDING,
		RectWidth(area),
		style.fontUi,
		style.GetBrushUiText());
		
	return posX;		
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Language Selector

static void OnClickLanguageSelector(void*, u64) {
	// @TODO
}


static void OnClickLanguagePopup(void*) {
	// @TODO
}

static f32 UpdateLanguageSelector(StatusBar* self, f32 posX, u64 i) {
		
	const Editor* focusedEditor = GetFocusedEditor();
	if (!focusedEditor) return posX;
	
	GlyphRun currentLanguageName {};
	currentLanguageName.Shape(focusedEditor->language ? focusedEditor->language->name : "None", style.fontUi);
	
	const bool l2r = IsL2R(self, i);
	const f32 width = currentLanguageName.width + PADDING_X4 + style.fontUi.lineHeight;
	const D2D_RECT_F area = GetArea(posX, width, l2r);
	
	if (focusedEditor->language && focusedEditor->language->HasLanguageServer()) {
		Style::Icon icon = Style::Icon_Unknown;
		D2D_COLOR_F color {};
		if (focusedEditor->language->server.state == LanguageServer::State_Standby) {
			icon = Style::Icon_Lsp_Standby;
			color = D2D1::ColorF(D2D1::ColorF::Black);
		} else if (focusedEditor->language->server.state == LanguageServer::State_Initializing) {
			icon = Style::Icon_Lsp_Initializing;
			color = D2D1::ColorF(D2D1::ColorF::DarkGreen);
		} else if (focusedEditor->language->server.state == LanguageServer::State_Running) {
			icon = Style::Icon_Lsp_Running;
			color = D2D1::ColorF(D2D1::ColorF::Green);
		} else if (focusedEditor->language->server.state == LanguageServer::State_ShuttingDown) {
			icon = Style::Icon_Lsp_ShuttingDown;
			color = D2D1::ColorF(D2D1::ColorF::LightGreen);
		} else if (focusedEditor->language->server.state == LanguageServer::State_Exited) {
			icon = Style::Icon_Lsp_Exited;
			color = D2D1::ColorF(D2D1::ColorF::Gray);
		} else if (focusedEditor->language->server.state == LanguageServer::State_Crashed) {
			icon = Style::Icon_Lsp_Crashed;
			color = D2D1::ColorF(D2D1::ColorF::Red);
		} else ASSERT_UNREACHABLE;
	
		style.brush->SetColor(color);
		
		deviceContext->FillRectangle(
			MakeRect(
				area.left,
				area.top,
				style.fontUi.lineHeight + PADDING_X2,
				style.fontUi.lineHeight + PADDING_X2),
			style.brush);
	
		deviceContext->DrawBitmap(
			style.icons[icon],
			MakeRect(
				area.left + PADDING,
				area.top + PADDING,
				style.fontUi.lineHeight,
				style.fontUi.lineHeight));
	}
	
	currentLanguageName.Draw(deviceContext, area.left + PADDING_X3 + style.fontUi.lineHeight, area.top + PADDING, style.fontUi, style.GetBrushUiText());
	
	if (mouse.Hittest(area, self, OnClickLanguageSelector))
		deviceContext->FillRectangle(area, style.GetBrushHover(mouse.isDown));
	
	return l2r ? area.right : area.left;
		
	/*
		const bool active = self->clickedElement == StatusBar::ElementType_LanguageSelector || mouse.isDown;
		
		
		self->hoveredElement = StatusBar::ElementType_LanguageSelector;
	}
	
	const bool drawPopup = self->clickedElement == StatusBar::ElementType_LanguageSelector
	                    || self->hoveredElement == StatusBar::ElementType_LanguageSelector;
	if (drawPopup) {
			
		// @TODO(tempmem)
		const u64 languageNamesCount = Language::languages.size() + 1u;
		GlyphRun* languageNames = new GlyphRun[languageNamesCount];
		DEFER(delete[] languageNames);
		
		f32 popupWidth = 0.0f;
		
		//
		// shape langauge names
		//
		{
			for (u64 i = 0u; i < languageNamesCount; i++) {
				GlyphRun& run = languageNames[i];
				const Language* language = Language::languages[i];
				
				if (language == focusedEditor->language) {
					run = std::move(currentLanguageName);
				} else {
					run.Shape(language->name, style.fontUi, &self->shapingMemory);
				}
				
				const f32 currentWidth = run.GetTotalAdvance();
				if (popupWidth < currentWidth)
					popupWidth = currentWidth;
			}
			
			
			GlyphRun& runNone = languageNames[Language::languages.size()];
			if (!focusedEditor->language) {
				runNone = std::move(currentLanguageName);
			} else {
				runNone.Shape("None", style.fontUi, &self->shapingMemory);
			}
			
			const f32 currentWidth = runNone.GetTotalAdvance();
			if (popupWidth < currentWidth)
				popupWidth = currentWidth;
		}
		
		const D2D_RECT_F areaPopup {
			.left   = l2r ? area.left : area.right - popupWidth,
			.top    = area.top - (languageNamesCount * (style.fontUi.lineHeight * PADDING_X2)),
			.right  = l2r ? area.left + popupWidth : area.right,
			.bottom = area.bottom};

		//
		// draw background
		//
		BlurArea(deviceContext, areaPopup);
		
		//
		// draw language names
		//
		for (u64 i = 0u; i < languageNamesCount; i++) {
			const D2D_RECT_F areaItem {
				.left = areaPopup.left,
				.top = areaPopup.top + (i * style.fontUi.lineHeight),
				.right = areaPopup.right,
				.bottom = areaPopup.top + ((i+1) * style.fontUi.lineHeight)};
			
			const GlyphRun& run = languageNames[i];
			const Language* language = i < Language::languages.size() ? Language::languages[i] : nullptr;
			
			run.Draw(deviceContext, {areaItem.left + PADDING, areaItem.top + PADDING}, style.fontUi, style.GetBrushUiText(language != nullptr));
			
			if (focusedEditor->language == language)
				deviceContext->DrawRectangle(areaItem, style.GetBrushUiText());
			
			if (mouse.Hittest(areaItem, OnClickLanguagePopup, reinterpret_cast<void*>(i)))
				deviceContext->DrawRectangle(areaItem, style.GetBrushHover(mouse.isDown));
		}
	}
	*/
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Diagnostics

static void OnClickDiagnostics(void* ud, u64) {
	auto editor = static_cast<Editor*>(ud);
	if (editor->toolWindow && editor->toolWindow->IsDiagnosticsList()) {
		delete editor->toolWindow;
		editor->toolWindow = nullptr;
	
	} else {
		delete editor->toolWindow;
		editor->toolWindow = EditorDiagnosticsList::Make(editor);
		if (!editor->toolWindow) {
			LogError("EditorDiagnosticsList::Make() failed");
		}
	}
}

static bool IsVisibleDiagnosticRecords() {
	Editor* editor = GetFocusedEditor();
	if (!editor) return false;
	
	std::scoped_lock lock {editor->editorDiagnostics.mutex};
	return !editor->editorDiagnostics.IsEmpty();
}

static f32 UpdateDiagnostics(StatusBar* self, f32 posX, u64 i) {
	
	Editor* focusedEditor = GetFocusedEditor();
	if (!focusedEditor) return posX;
	
	//
	// count records
	//
	u64 recordCounts[Diagnostics::Severity_MAX] {0u};
	{
		std::scoped_lock lock {focusedEditor->editorDiagnostics.mutex};
		if (focusedEditor->editorDiagnostics.IsEmpty()) return posX;
		
		for (const EditorDiagnostics::Record& record : focusedEditor->editorDiagnostics)
			recordCounts[record.severity]++;
	}		
	
	//
	// shape glyph runs
	//
	GlyphRun glyphRuns[Diagnostics::Severity_MAX] {};
	f32 totalWidth = 0.0f;
	{
		constexpr u64 bufferSize = 16;
		char buffer[bufferSize] {'\0'};
		
		for (int i = Diagnostics::Severity_Unknown; i < Diagnostics::Severity_MAX; i++) {
			
			const u64 recordCount = recordCounts[i];
			if (recordCount == 0u) continue;
			
			const std::to_chars_result result = std::to_chars(buffer, buffer + bufferSize, recordCount);
			if (result.ec != std::errc()) {
				LogWarning("failed to convert diagnostics count: %. Error: %", recordCount, FToCharsResult(result));
				continue;
			}
			
			const std::string_view text {buffer, result.ptr};
			
			GlyphRun& run = glyphRuns[i];
			run.Shape(text, style.fontUi);
			
			totalWidth += run.width + style.fontUi.lineHeight + PADDING_X3;
		}
	}
	
	const bool l2r = IsL2R(self, i);
	const D2D_RECT_F area = GetArea(posX, totalWidth, i);
	
	const bool diagnosticsListIsCurrentlyOpen = focusedEditor->toolWindow && focusedEditor->toolWindow->IsDiagnosticsList();
	if (diagnosticsListIsCurrentlyOpen)
		deviceContext->FillRectangle(area, style.GetBrushToggled());
	
	//
	// draw
	//
	{
		f32 offsetX = 0.0f;
		for (int i = Diagnostics::Severity_Unknown; i < Diagnostics::Severity_MAX; i++) {
			
			const u64 recordCount = recordCounts[i];
			if (recordCount == 0u) continue;
			
			deviceContext->DrawBitmap(
				style.icons[Diagnostics::SEVERITY_ICON_INDICIES[i]],
				MakeRect(
					area.left + offsetX + PADDING,
					mainWindow.height - style.fontUi.lineHeight - PADDING,
					style.fontUi.lineHeight,
					style.fontUi.lineHeight));
			
			//localPenX += PADDING_X2 + style.fontUi.lineHeight;
			
			GlyphRun& run = glyphRuns[i];
			run.Draw(deviceContext,
				area.left + offsetX + style.fontUi.lineHeight + PADDING_X2,
				mainWindow.height - style.fontUi.lineHeight - PADDING,
				style.fontUi,
				style.GetBrushUiText());
			
			offsetX += run.width + style.fontUi.lineHeight + PADDING_X3;
		}
		
		ASSERT(area.left + offsetX == area.right);
	}
	
	// @FIXME we pass the editor as the hotElement, check if this interferes in any way with the editor mouse logic
	// if not remove this FIXME 
	if (mouse.Hittest(area, focusedEditor, OnClickDiagnostics, diagnosticsListIsCurrentlyOpen)) {
		deviceContext->FillRectangle(area, style.GetBrushHover(mouse.isDown));
	}
	
	return l2r ? area.left : area.right;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Caret Info

struct CaretInfoText {
	struct ColorArea {
		u64 from = 0u;
		u64 to = 0u;
		D2D_COLOR_F color = {};
	};
	
	std::string text = {};
	
	ColorArea textColor[3];
	u64 textColorCount = 0u;
	
	u64 bgColorRangeIndex = U64_MAX;	 // index in textColor which contains the range
	D2D_COLOR_F bgColor = {};
};

static constexpr u64 NUMBER_BUFFER_SIZE = 20u; // max value 18446744073709551615 so 20 digits

static void AppendNumber(char* numberBuffer, CaretInfoText* info, u64 numberToFormat) {
	memset(numberBuffer, 0, NUMBER_BUFFER_SIZE * sizeof(char));
	
	const std::to_chars_result result = std::to_chars(numberBuffer, numberBuffer + NUMBER_BUFFER_SIZE, numberToFormat);
	if (result.ec != std::errc {}) {
		LogError("std::to_chars() failed. Error: %", FToCharsResult(result));
		return;
	}
	
	info->text.append(numberBuffer, result.ptr);
}

static void AppendWithColor(CaretInfoText* info, const char* str, const D2D_COLOR_F& color) {
	ASSERT(info->textColorCount < STATIC_ARRAY_SIZE(info->textColor));
	const u64 i = info->textColorCount++;
	info->textColor[i].from = info->text.size();
	info->text.append(str);
	info->textColor[i].to = info->text.size();
	info->textColor[i].color = color;
}

static void GetCaretInfoTextNormal(StatusBar* self, const TextController& controller, /*out*/ CaretInfoText* info) {
	ASSERT(controller.carets.size() == 1u);
	
	char numberBuffer[NUMBER_BUFFER_SIZE];

	// @TODO(tempmem) for info->text
	
	if (TextPosition from, to; controller.carets.front().GetSelection(&from, &to)) {
		
		info->textColorCount++;
		info->textColor[0].from = 0;
		
		u64 selectedBytes = 0u;
		u64 selectedLines = 0u;
		
		if (from.line == to.line) {
			selectedBytes = (to.column - from.column);
			selectedLines = 1u;
		} else {
			selectedBytes = controller.buffer.GetLineAt(from.line).LengthWithLinebreak() - from.column;
			for (u64 ln = from.line + 1u; ln < to.line - 1; ln++)
				selectedBytes += controller.buffer.GetLineAt(from.line).LengthWithLinebreak();
			selectedBytes += to.column;
			
			selectedLines = (to.line - from.line) + 1;
		}
			
		info->text.append(" ");
		AppendNumber(numberBuffer, info, selectedBytes);
		info->text.append(" chars ");
		AppendNumber(numberBuffer, info, selectedLines);
		info->text.append(" lines ");
		
		info->textColor[0].to = info->text.size();
		info->textColor[0].color = D2D_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f};
		
		info->bgColorRangeIndex = 0u;
		info->bgColor = style.colors[Style::Color_Selection];
		
		info->text.push_back(' ');
	}
		
	AppendWithColor(info, "Line ", style.colors[Style::Color_UiTextInactive]);
	AppendNumber(numberBuffer, info, controller.carets.front().position.line);
	
	AppendWithColor(info, " Char ", style.colors[Style::Color_UiTextInactive]);
	AppendNumber(numberBuffer, info, controller.carets.front().position.column);
	
}

static void GetCaretInfoTextEditCarets(StatusBar* self, const TextController& controller, /*out*/ CaretInfoText* info) {
	ASSERT(controller.isEditCaretsMode);
	
	char numberBuffer[NUMBER_BUFFER_SIZE];

	// @TODO(tempmem) for info->text
	
	info->textColorCount++;
	info->textColor[0].from = 0;
		
	info->text.append(" edit ");
	AppendNumber(numberBuffer, info, controller.carets.size());
	info->text.append(" carets ");
	
	info->textColor[0].to = info->text.size();
	info->textColor[0].color = D2D_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f};
	
	info->bgColorRangeIndex = 0u;
	info->bgColor = style.colors[Style::Color_EditorMultiCaretEdit];
	
	info->text.push_back(' ');
	
	AppendWithColor(info, "Line ", style.colors[Style::Color_UiTextInactive]);
	AppendNumber(numberBuffer, info, controller.carets.front().position.line);
	
	AppendWithColor(info, " Char ", style.colors[Style::Color_UiTextInactive]);
	AppendNumber(numberBuffer, info, controller.carets.front().position.column);
}


static void GetCaretInfoTextMultiCarets(StatusBar* self, const TextController& controller, /*out*/ CaretInfoText* info) {
	char numberBuffer[NUMBER_BUFFER_SIZE];
	
	AppendNumber(numberBuffer, info, controller.carets.size());
	AppendWithColor(info, " carets ", style.colors[Style::Color_UiTextInactive]);
}

static f32 UpdateCaretInfo(StatusBar* self, f32 posX, u64 i) {
	
	const Editor* focusedEditor = GetFocusedEditor();
	if (!focusedEditor) return posX;
	
	CaretInfoText caretInfoText {};
	if (focusedEditor->textController.isEditCaretsMode) {
		GetCaretInfoTextEditCarets(self, focusedEditor->textController, &caretInfoText);
	
	} else if (focusedEditor->textController.carets.size() > 1u) {
		GetCaretInfoTextMultiCarets(self, focusedEditor->textController, &caretInfoText);
	
	} else {
		GetCaretInfoTextNormal(self, focusedEditor->textController, &caretInfoText);	
	}
	
	GlyphRun run {};
	run.Shape(caretInfoText.text, style.fontUi);
	
	const bool l2r = IsL2R(self, i);
	const D2D_RECT_F area = GetArea(posX, run.width + PADDING_X2, l2r);
	const D2D_SIZE_F areaSize = RectSize(area);
		
	if (caretInfoText.bgColorRangeIndex < U64_MAX) {
		const CaretInfoText::ColorArea& clrArea = caretInfoText.textColor[caretInfoText.bgColorRangeIndex];
		
		f32 offsetFrom, offsetTo;
		run.MeasureOffsetRange(clrArea.from, clrArea.to, &offsetFrom, &offsetTo);
		
		style.brush->SetColor(caretInfoText.bgColor);
		deviceContext->FillRoundedRectangle(
			MakeRoundedRect(
				D2D_RECT_F {
					.left = area.left + PADDING + offsetFrom,
					.top = area.top + PADDING,
					.right = area.left + PADDING + offsetTo,
					.bottom = area.bottom - PADDING},
				RADIUS),
			style.brush);
	}
	
	ID2D1Bitmap* bitmapText = nullptr, *bitmapColor = nullptr;
	{
		ID2D1BitmapRenderTarget* renderTarget = CreateCompatibleRenderTarget(deviceContext, areaSize);
		if (!renderTarget) return posX;
		DEFER(renderTarget->Release());
	
		renderTarget->BeginDraw();
		renderTarget->Clear();
	
		run.Draw(renderTarget, PADDING, PADDING, style.fontUi, alphaMaskBrush);
		
		if (HRESULT hr = renderTarget->EndDraw(); hr != S_OK)
			LogError("EndDraw() failed for renderTargetColor. HRESULT: %", FHr(hr));
		
		renderTarget->GetBitmap(&bitmapText);
	}
	{
		ID2D1BitmapRenderTarget* renderTarget = CreateCompatibleRenderTarget(deviceContext, areaSize);
		if (!renderTarget) return posX;
	
		renderTarget->BeginDraw();
		renderTarget->Clear(style.colors[Style::Color_UiText]);
		DEFER(renderTarget->Release());
			
		for (u64 i = 0u; i < caretInfoText.textColorCount; i++) {
			const CaretInfoText::ColorArea& colorArea = caretInfoText.textColor[i];
			
			f32 offsetFrom, offsetTo;
			run.MeasureOffsetRange(colorArea.from, colorArea.to, &offsetFrom, &offsetTo);
			
			style.brush->SetColor(colorArea.color);
			
			renderTarget->FillRectangle(
				D2D_RECT_F {
					.left = PADDING + offsetFrom,
					.top = 0u,
					.right = PADDING + offsetTo,
					.bottom = areaSize.height},
				style.brush);
		}
			
		if (HRESULT hr = renderTarget->EndDraw(); hr != S_OK)
			LogError("EndDraw() failed for renderTargetColor. HRESULT: %", FHr(hr));
		
		renderTarget->GetBitmap(&bitmapColor);
	}
	
	BlendImages(deviceContext, {std::round(area.left), std::round(area.top)}, bitmapColor, bitmapText);
	
	bitmapColor->Release();
	bitmapText->Release();
	
	return l2r ? area.right : area.left;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Encoding Selector

static void OnClickEncodingSelector(void*, u64) {
}

static f32 UpdateEncodingSelector(StatusBar* self, f32 posX, u64 i) {
	staticGlyphRun.Shape("utf-8", style.fontUi);	
	
	const bool l2r = IsL2R(self, i);
	const D2D_RECT_F area = GetArea(posX, staticGlyphRun.width + PADDING_X2, l2r);
	staticGlyphRun.Draw(deviceContext, area.left + PADDING, area.top + PADDING, style.fontUi, style.GetBrushUiText());
	
	if (mouse.Hittest(area, self, OnClickEncodingSelector)) {
		deviceContext->FillRectangle(area, style.GetBrushHover(mouse.isDown));
	}
	
	return l2r ? area.right : area.left;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Line ending selector

static void OnClickLineEndingSelector(void*) {
}

static f32 UpdateLineEndingSelector(StatusBar* self, f32 posX, u64 i) {
	staticGlyphRun.Shape("Cr-Lf", style.fontUi);	
	
	const bool l2r = IsL2R(self, i);
	const D2D_RECT_F area = GetArea(posX, staticGlyphRun.width + PADDING_X2, l2r);
	staticGlyphRun.Draw(deviceContext, area.left + PADDING, area.top + PADDING, style.fontUi, style.GetBrushUiText());
	
	if (mouse.Hittest(area, self, OnClickEncodingSelector)) {
		deviceContext->FillRectangle(area, style.GetBrushHover(mouse.isDown));
	}
	
	return l2r ? area.right : area.left;
}

//#################################################################################################
// 
// S T A U S   B A R
//
//#################################################################################################

static constexpr std::string_view elementTypeNames[] {
	"none",
	"padding",
	"explorer",
	"console",
	"console-progress",
	"language",
	"diagnostics",
	"caret-info",
	"encoding",
	"line-ending"
};

static_assert(STATIC_ARRAY_SIZE(elementTypeNames) == StatusBar::ElementType_MAX);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr f32 (*elementUpdateFunctions[])(StatusBar*, f32, u64) {
	UpdateNone,
	UpdatePadding,
	UpdateExplorerButton,
	UpdateConsoleButton,
	UpdateConsoleProgress,
	UpdateLanguageSelector,
	UpdateDiagnostics,
	UpdateCaretInfo,
	UpdateEncodingSelector,
	UpdateLineEndingSelector,
};

static_assert(STATIC_ARRAY_SIZE(elementUpdateFunctions) == StatusBar::ElementType_MAX);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr bool (*elementIsVisibleFunctions[])() {
	IsVisibleTrue,
	IsVisibleTrue,
	IsVisibleTrue,
	IsVisibleTrue,
	IsVisibleConsoleProgress,
	IsVisibleTrue,
	IsVisibleDiagnosticRecords,
	IsVisibleTrue,
	IsVisibleTrue,
	IsVisibleTrue,
};

static_assert(STATIC_ARRAY_SIZE(elementIsVisibleFunctions) == StatusBar::ElementType_MAX);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StatusBar::Init() {
	elements = {
		ElementType_LanguageSelector,
		ElementType_Diagnostics,
		ElementType_ConsoleProgress,
		ElementType_LineEndingSelector,
		ElementType_EncodingSelector,
		ElementType_CaretInfo};
	l2rElementCount = 3u;
	return true;
}

void StatusBar::OnUpdate() {

	ID2D1SolidColorBrush* brushBg = nullptr;
	deviceContext->CreateSolidColorBrush(D2D1_COLOR_F {0.f, 0.f, 0.f, 1.0f}, &brushBg);
	DEFER(brushBg->Release());
	
	deviceContext->FillRectangle(
		D2D1_RECT_F {
			.left = 0,
			.top = mainWindow.height - style.fontUi.lineHeight - PADDING_X2,
			.right = mainWindow.width,
			.bottom = mainWindow.height},
		brushBg);
	
	f32 posX = 0;
	for (u64 i = 0u; i < elements.size(); i++) {
		const ElementType element = elements[i];
		
		bool drawSeperator = true;
		if (i == 0) drawSeperator = false;
		
		if (!elementIsVisibleFunctions[element]())
			drawSeperator = false;
		
		if (i == l2rElementCount) {
			drawSeperator = false;
			posX = mainWindow.width;
		}

		if (drawSeperator) {
			const bool l2r = IsL2R(this, i);
			posX = l2r
				? posX + PADDING
				: posX - PADDING;
			
			deviceContext->DrawLine(
				D2D_POINT_2F {
					.x = posX,
					.y = mainWindow.height - PADDING},
				D2D_POINT_2F {
					.x = posX,
					.y = mainWindow.height - PADDING - style.fontUi.lineHeight},
				style.GetBrushUiText(false));
				
			posX = l2r
				? posX + PADDING
				: posX - PADDING;
		}
		
		auto UpdateElement = elementUpdateFunctions[element];
		const f32 newPosX = UpdateElement(this, posX, i);
		
		posX = newPosX;
	}
}
