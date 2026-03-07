#include "editor.hh"
#include "globals.hh"
#include "main-window.hh"
#include "key-bindings.hh"

#include "graphics/effects.hh"
#include "language/language.hh"

#include "ui/constants.h"
#include "ui/style.hh"

#include "util/logging.hh"
#include "util/file-util.hh"
#include "util/rect-util.hh"

// @DUMMY
#include "graphics/glyph-run-harfbuzz.hh"
#include "graphics/glyph-run-dwrite.hh" 

#include "editor/editor-caretattached.hh"
#include "editor/editor-autocomplete.hh"
#include "editor/editor-diagnostics.hh"
#include "editor/editor-signaturehelp.hh"
#include "editor/editor-selectgototype.hh"
#include "editor/editor-search.hh"
#include "editor/editor-gotoline.hh"
#include "editor/editor-diagnosticslist.hh"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <windows.h>
//#include <guiddef.h>
#undef DrawText

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
constexpr float INSERT_ANIMATION_SPEED = 0.002f;
constexpr float CURSOR_ANIMATION_SPEED = 0.004f;
constexpr float CURSOR_ANIMATION_MAX_VALUE = (2.0f * F32_PI * 10.0f); // 10 cycles

#define LINENUMBERS_MAX_DIGITS 4

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Editor::Init() {
	
	if (!textController.InitForEditor(this)) {
		LogError("init text-editor failed");
		return false;
	}
	
	scrollarea.Init(0.f, 0.f, mainWindow.width, mainWindow.height, SCROLLBAR_WIDTH_WIDE);
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Editor::FileResult Editor::CloseFile() {
	
	//
	// save file
	//
	if (modified) {
		const UINT result = MessageBoxA(mainWindow.hWnd, "This buffer has unsaved changes.\nWould you like to save them?", "Unsaved changes", MB_YESNOCANCEL | MB_ICONWARNING);
		
		if (result == IDYES) {
			return SaveFile()
				? FileResult_Success
				: FileResult_Failure;
		
		} else if (result == IDCANCEL) {
			return FileResult_Canceled;

		} else {
			modified = false;
		}
	}

	ASSERT(!modified);
	
	//
	// close all the editor features
	//	
	if (editorCaretAttached) {
		editorCaretAttached->RemoveReference();
		editorCaretAttached = nullptr;
	}
	
	if (toolWindow) {
		delete toolWindow;
		toolWindow = nullptr;
	}
	
	editorDiagnostics.Reset();

	//
	// reset language
	//		
	if (language)
		language->OnCloseFile(this);
	
	language = nullptr;
	textDocumentIdentifier.uri.clear();
	textDocumentIdentifier.version = 0u;
	
	LogInfo("closed file: '%'", (!path.empty() ? path.c_str() : "(empty)"));
	path.clear();
	
	return FileResult_Success;
}

Editor::FileResult Editor::OpenFile(std::string path) {

	// close old file first
	if (auto fileResult = CloseFile(); fileResult != Editor::FileResult_Success)
		return fileResult;
	
	//
	// open file
	//
	HANDLE hFile = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%'. LastError: %", path, FLastErr(GetLastError()));
		return FileResult_Failure;
	}
	DEFER(CloseHandle(hFile));
	
	//
	// fill buffer
	//
	s64 fileSize = 0u;
	if (!GetFileSizeEx(hFile, reinterpret_cast<LARGE_INTEGER*>(&fileSize))) {
		LogError("GetFileSizeEx() failed. LastError: %", FLastErr(GetLastError()));
		return FileResult_Failure;
	}
	
	// @ISSUE
	// File size is a s64 but ReadFile only takes a DWORD aka u32
	// Does this mean we can only read 4GB in one go?
	ASSERT(fileSize < U32_MAX)
	
	std::string* buffer = this->textController.buffer.Clear();
	buffer->resize(fileSize);
		
	DWORD numOfBytesRead = 0;
	const bool ok = ReadFile(hFile, buffer->data(), static_cast<u32>(fileSize), &numOfBytesRead, nullptr);
	
	ASSERT(numOfBytesRead == fileSize);
	if (!ok) LogError("ReadFile() failed. Last Error: %", FLastErr(GetLastError()));
	
	//
	// reset some stuff
	//
	textController.buffer.RecreateLines();
	textController.Reset();
	scrollarea.ResetViewport();	
	
	//
	// set language
	//
	language = Language::GetLanguage(GetExtensionFromPath(path));
	if (language) {
		textDocumentIdentifier.uri = MakeUriFromPath(path);
		textDocumentIdentifier.version = 0u;
		language->OnOpenFile(this, *buffer);
	}
	this->path = std::move(path);
	
	//
	// prepare glyph runs
	//
	if (!GlyphRun_DWrite::ShapeBatch(this->textController.buffer, style.fontEditor, &glyphRuns)) {
		LogError("inital shaping failed!");
	}
	
	return FileResult_Success;
}

bool Editor::SaveFile() {
	
	if (!modified)
		return true;

	LogInfo("saving file '%'", path);
	
	const std::string tempFilename = FormatString("%.tmp", path);
	
	HANDLE hFile = CreateFileA(tempFilename.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("CreateFile() failed '%'. Last Error: %", tempFilename, FLastErr(GetLastError()));
		return false;
	}

	for (usize i = 0u; i < textController.buffer.LineCount(); i++) {
		const TextBuffer::Line& line = textController.buffer.lines[i];

		const std::string_view text = line.GetTextWithLinebreak();
		auto textSize = static_cast<DWORD>(text.size());
		
		DWORD bytesWritten = 0u;
		WriteFile(hFile, text.data(), textSize, &bytesWritten, NULL);
		ASSERT(bytesWritten == textSize);
	}

	CloseHandle(hFile);

	if (!ReplaceFileA(path.data(), tempFilename.c_str(), NULL, 0, NULL, NULL)) {
		LogError("ReplaceFile() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}
	
	modified = false;
	return true;
}

void Editor::ScrollToLine(u64 line) {

	if (editorCaretAttached) {
	 	editorCaretAttached->RemoveReference();
	 	editorCaretAttached = nullptr;
	}

	// scroll so that the target line roughly in the middle
	scrollTargetPosition = (style.fontEditor.lineHeight * line) - (scrollarea.vpSize.height * 0.4f);
	scrollTargetPosition = std::clamp(scrollTargetPosition, 0.0f, scrollarea.GetMaxPositionY());
	
	scrollSpeed = (scrollTargetPosition - scrollarea.vpY) / 30.0f;
	needsUpdate = true;
}

void Editor::ProcessTextChange(const TextChange* change) {
	if (!change) return;
	
	modified = true;
	textDocumentIdentifier.version++;
	
	bool needsFullReshape = false;
	{
		if (GetBuffer().LineCount() == glyphRuns.size())	 {
			ASSERT(change->count > 0u);
			const u64 line = change->operations[0].start.line;
			for (u64 i = 1; i < change->count; i++) {
				const TextChangeOperation& operation = change->operations[i];
				if (operation.start.line != line) {
					needsFullReshape = true;
					break;
				}
				
				if (!operation.insertedText.empty() && operation.insertionEnd.line != line) {
					needsFullReshape = true;
					break;
				}
				
				if (!operation.removedText.empty() && operation.removalEnd.line != line) {
					needsFullReshape = true;
					break;
				}
			}
		} else {
			needsFullReshape = true;
		}
	}
	
	if (needsFullReshape) {
		GlyphRun_DWrite::ShapeBatch(GetBuffer(), style.fontEditor, &glyphRuns);
	} else {
		const u64 line = change->operations[0].start.line;	
		glyphRuns[line].Shape(GetBuffer().GetLineAt(line).GetText(), style.fontEditor);
	}

	if (language)
		language->OnTextBufferChanged(this, change);
}

static float GetLineNumberWidth() {
	return std::ceil((style.fontEditor.GetSpaceAdvance() * LINENUMBERS_MAX_DIGITS) + PADDING_X4);
}

static D2D_POINT_2F TranslateTextPosition(const Editor* self, const TextPosition& position) {
	ASSERT(position.line < self->glyphRuns.size());
	const GlyphRun_DWrite& run = self->glyphRuns[position.line];

	return D2D_POINT_2F {
		.x = self->area.left + self->scrollarea.vpX + GetLineNumberWidth() + run.MeasureOffset(position.column),
		.y = self->area.top  - self->scrollarea.vpY + (position.line * style.fontEditor.lineHeight) };
}

D2D_POINT_2F Editor::GetCaretLocation() const {
	return TranslateTextPosition(this, textController.carets.front().position);
}

TextBuffer& Editor::GetBuffer() {
	return textController.buffer;
}

void Editor::GetVisibleLines(/*out*/ u64* pfirst, /*out*/ u64* plast) const {
	
	if (pfirst) {
		const auto first = static_cast<u64>((scrollarea.vpY) / style.fontEditor.lineHeight);
		*pfirst = std::max<u64>(0, first);
	}

	if (plast) {
		const auto last = static_cast<u64>((scrollarea.vpY + scrollarea.vpSize.height) / style.fontEditor.lineHeight);
		*plast = std::min<u64>(textController.buffer.GetMaxLine(), last);
	}
}

void Editor::PrepareInsertAnimation(u64 capacity /*= 0u*/) {	
	insertAnimationData.clear();
	insertAnimationData.reserve(capacity);
}

void Editor::AddInsertAnimationData(TextPosition from, TextPosition to) {
	insertAnimationData.push_back(InsertAnimationData {from, to});	
}

void Editor::StartInsertAnimation() {	
	insertAnimationOpacity = 1.0f;
	insertAnimationRunning = true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnClickEditor(void* ud, u64 uarg, s64 sarg) {
	auto self = reinterpret_cast<Editor*>(ud);
	const TextPosition clickedPosition {
		.line = uarg,
		.column = static_cast<u64>(sarg)};
	
	// @TODO Double and triple click logic
	// also alt + click should add carets
		
	if (self->textController.isEditCaretsMode) {
		self->textController.editCaretsPosition = clickedPosition;
	
	} else {
		self->textController.SetCaretPosition(clickedPosition);
	}
	
}

static void IterateGlyphRange(const Editor* self, TextPosition from, TextPosition to, ID2D1SolidColorBrush* brush, void (*funcAction) (f32 y, f32 from, f32 to, ID2D1SolidColorBrush* brush)) {

	const f32 x = self->area.left + self->scrollarea.vpX + GetLineNumberWidth();
	const f32 y = self->area.top  - self->scrollarea.vpY;
			
	// single line
	if (from.line == to.line) {

		const u64 line = std::clamp<u64>(from.line, 0u, self->glyphRuns.size() - 1);
		const GlyphRun_DWrite &glyphRun = self->glyphRuns[line];
		
		float offsetFrom, offsetTo;
		glyphRun.MeasureOffsetRange(from.column, to.column, &offsetFrom, &offsetTo);
		
		const float offsetY = y + (style.fontEditor.lineHeight * line);
		
		funcAction(offsetY, x + offsetFrom, x + offsetTo, brush);
				
	// multi-line
	} else {
		
		// handle first affected line
		{
			const GlyphRun_DWrite& glyphRun = self->glyphRuns[from.line];

			const float offsetFrom = x + glyphRun.MeasureOffset(from.column);
			const float offsetTo   = x + std::max(glyphRun.width, 2.0f);
			const float offsetY    = y + (style.fontEditor.lineHeight * from.line);

			funcAction(offsetY, offsetFrom, offsetTo, brush);
		}

		// handle all lines in between
		for (u64 i = from.line + 1u; i < to.line; i++) {
				
			const GlyphRun_DWrite& run = self->glyphRuns[i];
			
			const float offsetFrom = x + 0.0f;
			const float offsetTo   = x + std::max(run.width, 2.0f);
			const float offsetY    = y + (style.fontEditor.lineHeight * i);
			
			funcAction(offsetY, offsetFrom, offsetTo, brush);
		}

		// handle last line
		{
			const GlyphRun_DWrite& glyphRun = self->glyphRuns[to.line];

			const float offsetFrom = x + 0.f;
			const float offsetTo   = x + std::max(glyphRun.MeasureOffset(to.column), 2.0f);
			const float offsetY    = y + (style.fontEditor.lineHeight * to.line);

			funcAction(offsetY, offsetFrom, offsetTo, brush);
		}
	}
}

static void HighlightTextRange(f32 y, f32 from, f32 to, ID2D1SolidColorBrush* brush) {
	deviceContext->FillRoundedRectangle(
		D2D1_ROUNDED_RECT {
			.rect = D2D_RECT_F {
				.left   = from,
				.top    = y,
				.right  = to,
				.bottom = y + style.fontEditor.lineHeight },
			.radiusX = 2.f,
			.radiusY = 2.f },
		brush);
};

static void DrawScrollbarMarker(Editor* self, ID2D1DeviceContext* deviceContext, u64 line, f32 stroke, ID2D1SolidColorBrush* brush, /*out*/ f32* posY) {
	
	const f32 scrollbarPosY = self->area.top + (line * style.fontEditor.lineHeight) * self->scrollarea.GetRatio();
	deviceContext->DrawLine(
		D2D1_POINT_2F {
			.x = self->area.right - SCROLLBAR_WIDTH_WIDE,
			.y = scrollbarPosY},
		D2D1_POINT_2F {
			.x = self->area.right,
			.y = scrollbarPosY},
		brush,
		stroke);
		
	if (posY) *posY = scrollbarPosY; 
}

static void DrawDiagnosticsTooltip(Editor* self, ID2D1DeviceContext* deviceContext, const EditorDiagnostics::Record* record, ID2D1SolidColorBrush* severityBrush, bool isScrollbarTooltip) {
	
	//
	// shape the text
	//
	GlyphRun runCode {};
	GlyphRunMultiline runMessage {};
	
	runCode.Shape(record->code, style.fontEditor, &self->glyphRunShapingMemory);
	runMessage.Shape(record->message, style.fontUi, &self->glyphRunShapingMemory);
	
	//
	// measure the width and height
	//
	const f32 width = PADDING_X2 + std::max(
		runCode.GetTotalAdvance() + style.fontEditor.lineHeight + PADDING,
		runMessage.GetMaxAdvance());
		
	f32 height = PADDING_X2 + style.fontEditor.lineHeight
			   + PADDING_X2 + (style.fontUi.lineHeight * runMessage.GetLineCount());
			   
	if (isScrollbarTooltip)
		height += PADDING_X2 + (style.fontEditor.lineHeight * 4);		
	
	D2D1_POINT_2F position;
	
	if (!isScrollbarTooltip) {	
		const D2D1_POINT_2F curPos = self->GetCaretLocation();
		position = D2D_POINT_2F {
			.x = curPos.x,
			.y = curPos.y + style.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y };
	
	} else {		
		position = D2D_POINT_2F {
			.x = self->area.right - SCROLLBAR_WIDTH_WIDE - width - 15.0f,
			.y = self->area.top + (record->from.line * style.fontEditor.lineHeight * self->scrollarea.GetRatio()) - (height / 2.0f)};
	}
	
	const D2D_RECT_F area = MakeRect(position.x, position.y, width, height);	
	
	//
	// background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, area);
		if (!background) return;
		DEFER(background->Release());
	
		if (isScrollbarTooltip)
			DrawGlow(deviceContext, background, area);
	
		PushLayer(deviceContext, area);
		BlurArea(deviceContext, area, background);
	}
	
	DEFER(PopLayer(deviceContext));
	
	//
	// draw header
	//
	{
		deviceContext->FillRectangle(
			MakeRect(
				position.x,
				position	.y,
		    	width,
		    	style.fontEditor.lineHeight + PADDING_X2),
			style.GetBrushUiBackground());
	
		deviceContext->DrawBitmap(style.icons[Diagnostics::SEVERITY_ICON_INDICIES[record->severity]],
				MakeRect(
					position.x + PADDING,
					position.y + PADDING,
			    	style.fontEditor.lineHeight,
			    	style.fontEditor.lineHeight));
				
		runCode.Draw(deviceContext,
			D2D1_POINT_2F {
				.x = position.x + PADDING_X2 + style.fontEditor.lineHeight,
				.y = position.y + PADDING},
			style.fontEditor,
			style.GetBrushUiText());
	}
	
	//
	// draw seperator
	//
	{
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = position.x,
				.y = position.y + PADDING_X2 + style.fontEditor.lineHeight},
			D2D1_POINT_2F {
				.x = position.x + width,
				.y = position.y + PADDING_X2 + style.fontEditor.lineHeight},
			severityBrush);
	}
	
	//
	// draw message
	//
	{
		runMessage.Draw(deviceContext,
			D2D1_POINT_2F {
				.x = position.x + PADDING,
				.y = position.y + PADDING_X3 + style.fontEditor.lineHeight},
			style.fontUi,
			style.GetBrushUiText());
	}
	
	//
	// draw context
	//
	if (isScrollbarTooltip) {
	
		const f32 contextStartY = position.y + PADDING_X4
			+ style.fontEditor.lineHeight
	 		+ (style.fontUi.lineHeight * runMessage.GetLineCount());
		
		// draw 2nd seperator
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = position.x,
				.y = contextStartY},
			D2D1_POINT_2F {
				.x = position.x + width,
				.y = contextStartY},
			style.GetBrushUiBackground());
	
		const s64 sFrom = static_cast<s64>(record->from.line - 1);
		const s64 sTo   = static_cast<s64>(record->from.line + 2);
		for (s64 i = sFrom; i <= sTo; i++) {
			if (i < 0 || i >= static_cast<s64>(self->glyphRuns.size())) continue;
			
			self->glyphRuns[i].Draw(
				deviceContext,
				position.x + PADDING,
				contextStartY + (style.fontEditor.lineHeight * (i - sFrom)),
				style.fontEditor,
				style.GetBrushEditorText());
		}
		
		// draw underline in context
		if (record->from.line == record->to.line) {
			
			const GlyphRun_DWrite& glyphRun = self->glyphRuns[record->from.line];
			
			float offsetFrom, offsetTo;
			glyphRun.MeasureOffsetRange(record->from.column, record->to.column, &offsetFrom, &offsetTo);
						
			deviceContext->DrawLine(
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetFrom,
					.y = contextStartY},
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetTo,
					.y = contextStartY},
				style.GetBrushUiBackground());
		}
	}	
}

static TextPosition Hittest(Editor* self, f32 x, f32 y) {
	
	const float absoluteX = mouse.x + self->scrollarea.vpX - self->area.left - GetLineNumberWidth();
	const float absoluteY = mouse.y + self->scrollarea.vpY - self->area.top;
	ASSERT(absoluteY >= .0f);
	
	const u64 line = std::clamp(
		static_cast<u64>(absoluteY / style.fontEditor.lineHeight),
		0ull,
		self->textController.buffer.GetMaxLine());

	const GlyphRun_DWrite& run = self->glyphRuns[line];
	const u64 column = run.HitTest(absoluteX);
	
	return TextPosition {
		.line = line,
		.column = column};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Editor::OnUpdate() {

	//
	// reshape glyphs
	//
	{
		//GlyphRun_DWrite::ShapeBatch(GetBuffer(), style.fontEditor, &glyphRuns);

		scrollarea.totalSize.height = glyphRuns.size() * style.fontEditor.lineHeight;
		
		const bool isHot = mouse.Hittest(area, this);
		if (isHot) {
			const TextPosition mouseTextPosition = Hittest(this, mouse.x, mouse.y);
			
			if (mouse.event == Mouse::Event_Down) {
				mouse.StartDragging();
				textController.SetCaretPosition(mouseTextPosition);
			
			} else if (mouse.IsDragging()) {
				ASSERT(textController.carets.size() == 1u);
				
				TextController::Caret& caret = textController.carets.front();
				if (!caret.hasSelection && mouseTextPosition != caret.position) {
					const TextPosition& dragStartTextPosition = Hittest(this, mouse.dragStartX, mouse.dragStartY);
					caret.selection = dragStartTextPosition;
					caret.hasSelection = true;
				}
				
				caret.position = mouseTextPosition;
			}
		}
	}
	
	//
	// fill background
	//
	deviceContext->FillRectangle(area, style.GetBrushEditorBackground());

	//
	// draw scrollbar
	//
	{
		if (scrollSpeed != 0.0f) {
			auto isPastTargetPosition = scrollSpeed < 0.0f
				? [] (float vpY, float pos) { return vpY < pos; }
				: [] (float vpY, float pos) { return vpY > pos; };
			
			scrollarea.vpY += scrollSpeed;
			if (isPastTargetPosition(scrollarea.vpY, scrollTargetPosition)) {
				scrollarea.vpY = scrollTargetPosition;
				scrollSpeed = 0.0f;
			} else {
				needsUpdate = true;
			}
		}
		
		scrollarea.OnUpdate();
	}
	
	//
	// draw line numbers
	//
	{
		u16 digitGlyphIndicies[10];

		// get the glyph indicies for every possible digit
		{
			const auto digitChars = reinterpret_cast<const u32*>(U"0123456789");
			
			const HRESULT hr = style.fontEditor.fontFace->GetGlyphIndices(digitChars, 10, digitGlyphIndicies);
			if (hr != S_OK) {
				LogError("failed render line numbers. GetGlyphIndices() failed. HRESULT: %", FHr(hr));
				return;
			}
		}
		
		deviceContext->PushAxisAlignedClip(area, D2D1_ANTIALIAS_MODE_ALIASED);
		DEFER(deviceContext->PopAxisAlignedClip());

		// render all line numbers
		for (u64 i = 0u; i < glyphRuns.size(); i++) {
	
			u16 glyphsToRender[LINENUMBERS_MAX_DIGITS];
			std::fill_n(glyphsToRender, LINENUMBERS_MAX_DIGITS, style.fontEditor.glyphIndexSpace);

			// get glyphs to render
			{
				u64 remainingNumber = i + 1; // line numbers are 1-based
				s16 glyphsToRenderIndex = LINENUMBERS_MAX_DIGITS - 1;

				while (true) {

					const int digit = remainingNumber % 10;
					glyphsToRender[glyphsToRenderIndex] = digitGlyphIndicies[digit];

					glyphsToRenderIndex--;
					if (glyphsToRenderIndex < 0)
						break;
					
					remainingNumber /= 10;
					if (remainingNumber == 0u)
						break;
				}
			}

			// draw the glyphs
			{
				DWRITE_GLYPH_RUN glyphRun {};
				glyphRun.fontFace = style.fontEditor.fontFace;
				glyphRun.fontEmSize = style.fontEditor.size;
				glyphRun.glyphCount = LINENUMBERS_MAX_DIGITS;
				glyphRun.glyphIndices = glyphsToRender;
				
				ID2D1SolidColorBrush* brush;
				for (const TextController::Caret& caret : textController.carets) {
					if (caret.position.line == i) {
						brush = style.GetBrushEditorText();
						goto draw;
					}
				}
				brush = style.GetBrushUiText(false);
			
			draw:
				deviceContext->DrawGlyphRun(
					D2D_POINT_2F {
						.x = area.left,
						.y = area.top + (style.fontEditor.lineHeight * i) - scrollarea.vpY + style.fontEditor.baselineOffset },
					&glyphRun,
					brush);
			}
		}
	}

	//
	// draw text
	//
	{
		ID2D1Bitmap* bitmapGlyphs, *bitmapColor;

		const float lineNumbersWidth = GetLineNumberWidth();
		const D2D_SIZE_F textAreaSize {
			.width  = RectWidth(area) - lineNumbersWidth,
			.height = RectHeight(area) };
		const D2D1_MATRIX_3X2_F scrollTransform = D2D1::Matrix3x2F::Translation(scrollarea.vpX, -scrollarea.vpY);

		// render glyphs to bitmap
		{
			ID2D1BitmapRenderTarget* renderTargetText = CreateCompatibleRenderTarget(deviceContext, textAreaSize);
			if (!renderTargetText) return;
			DEFER(renderTargetText->Release());
			
			renderTargetText->BeginDraw();
			renderTargetText->Clear();
			renderTargetText->SetTransform(scrollTransform);
						
			for (u64 i = 0; i < glyphRuns.size(); i++) {
				const GlyphRun_DWrite& glyphRun = glyphRuns[i];
				
				const f32 offsetY = (i * style.fontEditor.lineHeight);

				glyphRun.Draw(renderTargetText, 0.0f, offsetY, style.fontEditor, alphaMaskBrush);
			}
			
			if (HRESULT hr = renderTargetText->EndDraw(); hr != S_OK) {
				LogError("EndDraw() failed for renderTargetText. HRESULT: %", FHr(hr));
				return;
			}

			renderTargetText->GetBitmap(&bitmapGlyphs);
		}
		
		DEFER(bitmapGlyphs->Release());

		// @TODO we could just reuse the other rendertarget??
		// apply syntax highlighting
		{
			ID2D1BitmapRenderTarget* renderTargetColor = nullptr;
			if (HRESULT hr = mainWindow.deviceContext->CreateCompatibleRenderTarget(textAreaSize, &renderTargetColor); hr != S_OK) {
				LogError("CreateCompatibleRenderTarget() failed. HRESULT: %", FHr(hr));
				return;
			}
			DEFER(renderTargetColor->Release());

			renderTargetColor->BeginDraw();
			renderTargetColor->Clear(style.colors[Style::Color_EditorText]);
			renderTargetColor->SetTransform(scrollTransform);

			if (language) {
				u64 firstVisible, lastVisible;
				GetVisibleLines(&firstVisible, &lastVisible);
				
				language->HighlightSyntax(this, renderTargetColor, firstVisible, lastVisible);
			}

			if (HRESULT hr = renderTargetColor->EndDraw(); hr != S_OK) {
				LogError("EndDraw() failed for renderTargetColor. HRESULT: %", FHr(hr));
				return;
			}

			renderTargetColor->GetBitmap(&bitmapColor);
		}
		
		DEFER(bitmapColor->Release());

		// blend color and text together		
		BlendImages(deviceContext, D2D_POINT_2F {area.left + lineNumbersWidth, area.top}, bitmapColor, bitmapGlyphs);
	}

	//
	// draw carets
	//	
	{
		f32 fillingOpacity = NAN;
		if (cursorBlinkValue < CURSOR_ANIMATION_MAX_VALUE) {
			fillingOpacity = std::cos(cursorBlinkValue + F32_PI) * 0.5f + 0.5f;
			cursorBlinkValue += CURSOR_ANIMATION_SPEED * deltaTime;
			needsUpdate = true;
		}
		
		const D2D_SIZE_F caretSize {
			.width  = style.fontEditor.GetSpaceAdvance(),
			.height = style.fontEditor.lineHeight};
		
		ID2D1SolidColorBrush* brushCaret = style.GetBrushEditorText();
		for (const TextController::Caret& caret : textController.carets) {
			
			// draw selection
			if (TextPosition selFrom, selTo; caret.GetSelection(&selFrom, &selTo)) {
				ID2D1SolidColorBrush* brush = style.GetBrushSelection();
				
				IterateGlyphRange(this, selFrom, selTo, brush, HighlightTextRange);
			}
			
			// draw caret
			const D2D_RECT_F caretRect =  MakeRect(
				TranslateTextPosition(this, caret.position),
				caretSize);
				
			ID2D1SolidColorBrush* brushCaret = style.GetBrushEditorText();			
			deviceContext->DrawRectangle(caretRect, brushCaret);
			
			// draw blink animation
			if (!std::isnan(fillingOpacity) && !textController.isEditCaretsMode) {
				const f32 before = brushCaret->GetOpacity();
				brushCaret->SetOpacity(fillingOpacity);
				deviceContext->FillRectangle(caretRect, brushCaret);
				brushCaret->SetOpacity(before);
			}
		}
		
		if (textController.isEditCaretsMode) {
			
			ID2D1SolidColorBrush* brushEditCaret = style.GetBrushEditorMultiCaretEdit();
			const D2D_RECT_F caretRect = MakeRect(
				TranslateTextPosition(this, textController.editCaretsPosition),
				caretSize);
			
			if (!std::isnan(fillingOpacity)) {
				brushEditCaret->SetOpacity(fillingOpacity);
				deviceContext->FillRectangle(caretRect, style.GetBrushEditorMultiCaretEdit());
				brushEditCaret->SetOpacity(1.0f);
			}
			
			for (const TextController::Caret& caret : textController.carets) {
				if (caret.position == textController.editCaretsPosition) {
					brushEditCaret = style.GetBrushEditorText();
					break;
				}
			}
			
			deviceContext->DrawRectangle(caretRect, brushEditCaret);
		}
	}

	//
	// search results
	//
	if (toolWindow && toolWindow->IsSearch()) {
	
		EditorSearch* search = static_cast<EditorSearch*>(toolWindow);
		if (search->IsSearchComplete()) {
		
			const float offsetX = area.left + GetLineNumberWidth() + scrollarea.vpX;
			const float offsetY = area.top - scrollarea.vpY;
	
			for (const EditorSearch::SearchResult& result : search->threadData->results) {
				
				const GlyphRun_DWrite& run = glyphRuns[result.from.line];
				
				deviceContext->DrawRectangle(
					D2D1_RECT_F {
						.left   = offsetX + run.MeasureOffset(result.from.column),
						.top    = offsetY + result.from.line * style.fontEditor.lineHeight,
						.right  = offsetX + run.MeasureOffset(result.to.column),
						.bottom = offsetY + (result.from.line + 1) * style.fontEditor.lineHeight },
					style.GetBrushEditorText());
			}
		}
	}

	//
	// insert-animation
	//
	if (insertAnimationRunning) {
		ID2D1SolidColorBrush* insertAnimBrush = style.GetBrushSelection();
		insertAnimBrush->SetOpacity(insertAnimationOpacity);
		DEFER(insertAnimBrush->SetOpacity(1.0f));
		
		
		for (const InsertAnimationData& insertData : insertAnimationData)
			IterateGlyphRange(this, insertData.from, insertData.to, insertAnimBrush, HighlightTextRange);
		
		insertAnimationOpacity -= deltaTime * INSERT_ANIMATION_SPEED;
		if (insertAnimationOpacity < .0f) {
			insertAnimationData.clear();
			insertAnimationRunning = false;
		} else {
			needsUpdate = true;
		}
	}

	//
	// draw diagnostics
	//
	{
		ID2D1SolidColorBrush* severityBrush = nullptr;
		if (HRESULT hr = deviceContext->CreateSolidColorBrush(D2D_COLOR_F {}, &severityBrush); hr != S_OK) {
			LogError("CreateSolidColorBrush() failed. HRESULT: %", FHr(hr));
			return;
		}
		DEFER(severityBrush->Release());
		
		const bool mouseIsOnScrollbarArea = RectContains(
			D2D_RECT_F {
				.left = area.right - SCROLLBAR_WIDTH_WIDE,
				.top = area.top,
				.right = area.right,
				.bottom = area.bottom},
			mouse.x,
			mouse.y);
		
		// @TODO(settings)
		f32 minDistanceToMarker = 10.0f;
		const EditorDiagnostics::Record* hoveredRecordOnScrollbar = nullptr;
		
		const std::scoped_lock lock {editorDiagnostics.mutex};
		for (const EditorDiagnostics::Record& record : editorDiagnostics) {

			severityBrush->SetColor(Diagnostics::SEVERITY_COLORS[record.severity]);
			
			// draw underlines

			IterateGlyphRange(this, record.from, record.to, severityBrush, [] (f32 offsetY, f32 offsetFrom, f32 offsetTo, ID2D1SolidColorBrush* brush) {
				deviceContext->DrawLine(
					D2D_POINT_2F {
						.x = offsetFrom,
						.y = offsetY + style.fontEditor.underlineOffset},
					D2D_POINT_2F { 
						.x = offsetTo,
						.y = offsetY + style.fontEditor.underlineOffset},
					brush,
					2.0f,
					nullptr);
			});
			
			// draw error details-window
			
			const bool showErrorDetails = !editorCaretAttached &&
			                              !textController.HasSelection() &&
			                              !textController.isEditCaretsMode &&
			                              (textController.carets.size() == 1u) &&
			                              (record.from <= textController.carets.front().position && record.to > textController.carets.front().position);
			
			if (showErrorDetails)
				DrawDiagnosticsTooltip(this, deviceContext, &record, severityBrush, false);
			
			// draw scrollbar marker
			
			f32 scrollbarPosY = 0.0f;
			DrawScrollbarMarker(this, deviceContext, record.from.line, 1.0f, severityBrush, &scrollbarPosY);
			
			if (mouseIsOnScrollbarArea) {
				// @TODO(mouse)
				/*
				const f32 distanceToMouse = std::abs(mouse.y - scrollbarPosY);
				if (mouse.Hittest("Scrollarea.Marker", minDistanceToMarker > distanceToMouse)) {
					minDistanceToMarker = distanceToMouse;
					hoveredRecordOnScrollbar = &record;
				}
				*/
			}
		}
		
		// draw hovered record on scrollbar if any
		if (hoveredRecordOnScrollbar) {
			severityBrush->SetColor(Diagnostics::SEVERITY_COLORS[hoveredRecordOnScrollbar->severity]);
			DrawDiagnosticsTooltip(this, deviceContext, hoveredRecordOnScrollbar, severityBrush, true);
			DrawScrollbarMarker(this, deviceContext, hoveredRecordOnScrollbar->from.line, 2.0f, severityBrush, nullptr);
			
			// @TODO(mouse)	
			//if (mouse.IsClicked())
				ScrollToLine(hoveredRecordOnScrollbar->from.line);
		}
	}
	
	//
	// editor features
	//
	{
		if (editorCaretAttached)
		 	editorCaretAttached->OnUpdate();
		 	
		if (toolWindow)
			toolWindow->OnUpdate();
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Editor::OnResize(D2D1_RECT_F newArea) {
	this->area = newArea;
	this->scrollarea.OnResize(newArea);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Input

void Editor::OnChar(const char* data, u64 len) {
	
	if (toolWindow && toolWindow->OnChar(data, len))
		return;
	
	TextChange* change = nullptr;
	textController.OnChar(data, len, &change);

	ProcessTextChange(change);

	if (editorCaretAttached)
 		editorCaretAttached->OnInput();
 	
	cursorBlinkValue = 0.0f;	
}

void Editor::OnMouseWheel(f32 distance) {
	// @TODO(settings) scroll distance
	scrollarea.ScrollVertical(distance * style.fontEditor.lineHeight * 5);
}

void Editor::OnKeyDown(KeyEvent event) {

	if (event.vkeycode == VK_ESCAPE && event.NoModifiers()) {

		if (toolWindow) {
			delete toolWindow;
			toolWindow = nullptr;
		
		} else if (editorCaretAttached) {
		 	editorCaretAttached->RemoveReference();
		 	editorCaretAttached = nullptr;
		 	return;
		
		}
	}
			
	if (event == keybinds.actions.openSearch || event == keybinds.actions.openSearchAndReplace) {
	
		const bool showReplace = event == keybinds.actions.openSearchAndReplace;
		
		if (auto search = dynamic_cast<EditorSearch*>(toolWindow)) {
			search->ToggleReplaceTextbox(showReplace);

		} else {
			delete toolWindow;
			
			toolWindow = EditorSearch::Make(this, showReplace);
			if (!toolWindow)
				LogError("EditorSearch::Make() failed");
		}
		return;
			
	} else if (event == keybinds.actions.openGotoLine) {
		
		if (toolWindow && toolWindow->IsGotoLine()) {
			delete toolWindow;
			toolWindow = nullptr;
		
		} else {
			delete toolWindow;
			
			toolWindow = EditorGotoLine::Make(this);
			if (!toolWindow)
				LogError("EditorGotoLine::Make() failed");
		}
		return;
	
	// @TODO keybind
	} else if (event.vkeycode == VK_OEM_PLUS && event.ctrl && !event.alt) {
		
		if (toolWindow && toolWindow->IsDiagnosticsList()) {
			delete toolWindow;
			toolWindow = nullptr;
		
		} else {
			delete toolWindow;
			
			toolWindow = EditorDiagnosticsList::Make(this);
			if (!toolWindow)
				LogError("EditorDiagnosticsList::Make() failed");
		}
		return;
	
	} else if (event == keybinds.actions.showGotoLocation) {
		if (!language) return;
			
		if (editorCaretAttached) {
			editorCaretAttached->RemoveReference();
			editorCaretAttached = nullptr;
		}
		
    	editorCaretAttached = EditorSelectGotoType::Make(this);
    	return;
	
	} else if (event == keybinds.actions.showSignatureHelp) {
		if (!language) return;	
		
		if (editorCaretAttached) {
			editorCaretAttached->RemoveReference();
			editorCaretAttached = nullptr;
		}
		
    	editorCaretAttached = language->GetSignatureHelp(this);
    	return;
    	
	} else if (event == keybinds.actions.showAutocomplete) {
		if (!language) return;
		
		EditorSignatureHelp* signatureHelp = dynamic_cast<EditorSignatureHelp*>(editorCaretAttached);
		if (signatureHelp) signatureHelp->AddReference();
		DEFER(if (signatureHelp) signatureHelp->RemoveReference());
		
		if (editorCaretAttached) {
			editorCaretAttached->RemoveReference();
			editorCaretAttached = nullptr;
		}
		
		EditorAutocomplete* autocomplete = language->GetAutoComplete(this);
		if (autocomplete && signatureHelp) {
			autocomplete->signatureHelp = signatureHelp;
			signatureHelp->AddReference();
			signatureHelp->autocompleteIsActive = true;
			
		}
		
		editorCaretAttached = autocomplete;
		return;
	
	} else if (event == keybinds.actions.saveFile) {
		SaveFile();
		return;
	
	} else if (event == keybinds.actions.scrollUp) {
		scrollarea.vpY -= (style.fontEditor.lineHeight * 2);
		if (scrollarea.vpY < 0.0f)
			scrollarea.vpY = 0.0f;
		return;
	
	} else if (event == keybinds.actions.scrollDown) {
		scrollarea.vpY += (style.fontEditor.lineHeight * 2);
		if (scrollarea.vpY >= scrollarea.GetMaxPositionY())
			scrollarea.vpY  = scrollarea.GetMaxPositionY();
		return;
	}
	
	if (toolWindow && toolWindow->OnKeyDown(event)) {
		return;
	}
	
	if (editorCaretAttached && editorCaretAttached->OnKeyDown(event))
		return;
			
	if (TextChange* change = nullptr; textController.OnKeyDown(event, &change)) {

		ProcessTextChange(change);
	
 		if (editorCaretAttached)
			editorCaretAttached->OnInput();
		
		cursorBlinkValue = 0u;
	}
}

bool Editor::OnClose() {
	if (modified) {
		const FileResult closeResult = CloseFile();
		if (closeResult == FileResult::FileResult_Failure)
			return false;
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Editor::~Editor() noexcept {

	if (editorCaretAttached)
	 	editorCaretAttached->RemoveReference();

	delete toolWindow;
}