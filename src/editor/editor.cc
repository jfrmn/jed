#include "editor.hh"
#include "globals.hh"
#include "main-window.hh"
#include "settings.hh"

#include "graphics/effects.hh"
#include "language/language.hh"
#include "ui/constants.h"

#include "util/logging.hh"
#include "util/file-util.hh"
#include "util/rect-util.hh"

#include "editor/editor-caretattached.hh"
#include "editor/editor-autocomplete.hh"
#include "editor/editor-diagnostics.hh"
#include "editor/editor-signaturehelp.hh"
#include "editor/editor-selectgototype.hh"
#include "editor/editor-search.hh"
#include "editor/editor-gotoline.hh"
#include "editor/editor-diagnosticslist.hh"

#include <algorithm>

#include <tree_sitter/api.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <windows.h>
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Closing and Opening
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void UnloadFile(Editor* self) {
	
	//
	// close all the editor features
	//	
	if (self->editorCaretAttached) {
		self->editorCaretAttached->RemoveReference();
		self->editorCaretAttached = nullptr;
	}
	
	if (self->toolWindow) {
		delete self->toolWindow;
		self->toolWindow = nullptr;
	}
	
	self->editorDiagnostics.Reset();

	//
	// reset language
	//		
	if (self->language)
		self->language->OnCloseFile(self);
	
	//
	// reset lwt
	//
	self->lastWriteTime = {};
	
	LogInfo("closed file: '%'", (!self->path.empty() ? self->path.c_str() : "(empty)"));
}

static bool LoadFile(Editor* self) {
	
	LogInfo("opening file '%'", self->path);
	
	//
	// open file
	//
	HANDLE hFile = CreateFileA(self->path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%'. LastError: %", self->path, FLastErr(GetLastError()));
		return false;
	}
	DEFER(CloseHandle(hFile));
	
	//
	// fill buffer
	//
	s64 fileSize = 0u;
	if (!GetFileSizeEx(hFile, reinterpret_cast<LARGE_INTEGER*>(&fileSize))) {
		LogError("GetFileSizeEx() failed. LastError: %", FLastErr(GetLastError()));
		return false;
	}
	
	// @ISSUE
	// File size is a s64 but ReadFile only takes a DWORD aka u32
	// Does this mean we can only read 4GB in one go?
	ASSERT(fileSize < U32_MAX)
	
	std::string* buffer = self->textController.buffer.Clear();
	buffer->resize(fileSize);
		
	DWORD numOfBytesRead = 0;
	const bool ok = ReadFile(hFile, buffer->data(), static_cast<u32>(fileSize), &numOfBytesRead, nullptr);
	
	ASSERT(numOfBytesRead == fileSize);
	if (!ok) LogError("ReadFile() failed. Last Error: %", FLastErr(GetLastError()));
	
	self->textController.buffer.RecreateLines();
	
	//
	// set lwt
	//
	GetFileTime(hFile, nullptr, nullptr, &self->lastWriteTime);
	
	//
	// prepare glyph runs
	//
	if (!GlyphRun::ShapeBatch(self->textController.buffer, settings.fontEditor, &self->glyphRuns)) {
		LogError("inital shaping failed!");
	}
	
	if (self->language)
		self->language->OnOpenFile(self, *buffer);
	
	return true;
}

Editor::FileResult Editor::CloseFile() {
	
	if (isDirty) {
		const UINT result = MessageBoxA(mainWindow.hWnd, "This buffer has unsaved changes.\nWould you like to save them?", "Unsaved changes", MB_YESNOCANCEL | MB_ICONWARNING);
		
		if (result == IDYES) {
			return SaveFile()
				? FileResult_Success
				: FileResult_Failure;
		
		} else if (result == IDCANCEL) {
			return FileResult_Canceled;

		} else {
			isDirty = false;
		}
	}

	ASSERT(!isDirty);
	
	UnloadFile(this);
	
	language = nullptr;
	textDocumentIdentifier.uri.clear();
	textDocumentIdentifier.version = 0u;		
	path.clear();	
	
	return FileResult_Success;
}

Editor::FileResult Editor::OpenFile(std::string path) {

	// close old file first
	if (auto fileResult = CloseFile(); fileResult != FileResult_Success)
		return fileResult;
			
	//
	// set language
	//
	language = Language::GetLanguage(GetExtensionFromPath(path));
	textDocumentIdentifier.uri = MakeUriFromPath(path);	
	this->path = std::move(path);
	
	if (!LoadFile(this)) {
		this->path.clear();	
		language = nullptr;
		return FileResult_Failure;
	}
			
	//
	// reset some stuff
	//
	textController.Reset();
	scrollarea.ResetViewport();	
	
	return FileResult_Success;
}

bool Editor::SaveFile() {
	
	if (!isDirty && !fileRemoved)
		return true;

	LogInfo("saving file '%'", path);
	
	if (settings.backupFileBeforeSaving) {
		// @TODO
	}
	
	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("CreateFile() failed '%'. Last Error: %", path.c_str(), FLastErr(GetLastError()));
		return false;
	}
	
	for (u64 i = 0u; i < textController.buffer.LineCount(); i++) {
		const TextBuffer::Line& line = textController.buffer.lines[i];

		const std::string_view text = line.GetTextWithLinebreak();
		auto textSize = static_cast<DWORD>(text.size());
		
		DWORD bytesWritten = 0u;
		WriteFile(hFile, text.data(), textSize, &bytesWritten, NULL);
		ASSERT(bytesWritten == textSize);
	}

	CloseHandle(hFile);
	
	hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("Failed to reopen file. CreateFile() failed '%'. Last Error: %", path.c_str(), FLastErr(GetLastError()));
		return false;
	}	
	GetFileTime(hFile, nullptr, nullptr, &lastWriteTime);
	CloseHandle(hFile);
	
	isDirty = false;
	
	if (settings.backupFileBeforeSaving) {
		// @TODO
	}
	
	return true;
}

void Editor::OnFileChanged(const FileChangeRecord* fileChangeRecord) {
	
	if (fileChangeRecord->action == FileChangeRecord::Action_Added) {
		fileRemoved = false;
	
	} else if (fileChangeRecord->action == FileChangeRecord::Action_Removed) {
		fileRemoved = true;
			
	} else if (fileChangeRecord->action == FileChangeRecord::Action_Modified) {
		
		HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE) {
			LogError("CreateFileA() failed. '%'. Last Error: %", path.c_str(), FLastErr(GetLastError()));
			return;
		}
		DEFER(CloseHandle(hFile));
		
		FILETIME currentLastWriteTime = {};
		GetFileTime(hFile, nullptr, nullptr, &currentLastWriteTime);
			
		LogDetail("checking file modification for '%'...", path);
		
		if (lastWriteTime.dwLowDateTime == currentLastWriteTime.dwLowDateTime && lastWriteTime.dwHighDateTime == currentLastWriteTime.dwHighDateTime) {
			LogDetail("file was not modified.");
			return;
		}
				
		LogInfo("file was modified externally.");
		
		if (isDirty) {
			const UINT result = MessageBoxA(mainWindow.hWnd,
				"File has been modified externally.\nWould like to discard all local modification and reload the file?",
				"File modified",
				MB_ICONQUESTION | MB_YESNO);
			
			if (result == IDNO) {
				LogDetail("file will not be reloaded");
				lastWriteTime = currentLastWriteTime;
				return;
			}
			
			isDirty = false;
		}
		
		LogInfo("reloading file");
		
		UnloadFile(this);
		LoadFile(this);
		
		const u64 newLine = std::min(textController.carets.front().position.line, textController.buffer.GetMaxLine());
		textController.SetCaretPosition(TextPosition {
			.line   = newLine,
			.column = std::min(textController.carets.front().position.column, textController.buffer.GetLineAt(newLine).length)});
		scrollarea.vpX = std::min(scrollarea.vpX, scrollarea.GetMaxPositionX());
		scrollarea.vpY = std::min(scrollarea.vpY, scrollarea.GetMaxPositionY());	
	
	} else if (fileChangeRecord->action == FileChangeRecord::Action_RenamedNew) {
		
		if (language)
			language->OnCloseFile(this);
		
		path = std::string(fileChangeRecord->filename, fileChangeRecord->filenameLength);	
		textDocumentIdentifier.uri = MakeUriFromPath(path);
			
		const std::string buffer = textController.buffer.GetText();
		
		if (language)
			language->OnOpenFile(this, buffer);
	
	} else {
		LogError("Unexpected file changed action: %", fileChangeRecord->action);
		ASSERT_UNREACHABLE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Misc
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void Editor::ScrollToLine(u64 line) {

	if (editorCaretAttached) {
	 	editorCaretAttached->RemoveReference();
	 	editorCaretAttached = nullptr;
	}

	// scroll so that the target line roughly in the middle
	scrollTargetPosition = (settings.fontEditor.lineHeight * line) - (scrollarea.vpSize.height * 0.4f);
	scrollTargetPosition = std::clamp(scrollTargetPosition, 0.0f, scrollarea.GetMaxPositionY());
	
	scrollSpeed = (scrollTargetPosition - scrollarea.vpY) / 30.0f;
	needsUpdate = true;
}

void Editor::ProcessTextChange(const TextChange* change) {
	if (!change) return;
	
	isDirty = true;
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
		GlyphRun::ShapeBatch(GetBuffer(), settings.fontEditor, &glyphRuns);
	} else {
		const u64 line = change->operations[0].start.line;	
 		glyphRuns[line].Shape(GetBuffer().GetLineAt(line).GetText(), settings.fontEditor);
	}

	if (language)
		language->OnTextBufferChanged(this, change);
}

static float GetLineNumberWidth() {
	return std::ceil((settings.fontEditor.GetSpaceAdvance() * LINENUMBERS_MAX_DIGITS) + PADDING_X4);
}

static D2D_POINT_2F TranslateTextPosition(const Editor* self, const TextPosition& position) {
	ASSERT(position.line < self->glyphRuns.size());
	const GlyphRun& run = self->glyphRuns[position.line];

	return D2D_POINT_2F {
		.x = self->area.left + self->scrollarea.vpX + GetLineNumberWidth() + run.MeasureOffset(position.column),
		.y = self->area.top  - self->scrollarea.vpY + (position.line * settings.fontEditor.lineHeight) };
}

D2D_POINT_2F Editor::GetCaretLocation() const {
	return TranslateTextPosition(this, textController.carets.front().position);
}

TextBuffer& Editor::GetBuffer() {
	return textController.buffer;
}

void Editor::GetVisibleLines(/*out*/ u64* pfirst, /*out*/ u64* plast) const {
	
	if (pfirst) {
		const auto first = static_cast<u64>((scrollarea.vpY) / settings.fontEditor.lineHeight);
		*pfirst = std::max<u64>(0, first);
	}

	if (plast) {
		const auto last = static_cast<u64>((scrollarea.vpY + scrollarea.vpSize.height) / settings.fontEditor.lineHeight);
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Drawing
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void IterateGlyphRange(const Editor* self, TextPosition from, TextPosition to, void (*funcAction) (f32 y, f32 from, f32 to)) {

	const f32 x = self->area.left + self->scrollarea.vpX + GetLineNumberWidth();
	const f32 y = self->area.top  - self->scrollarea.vpY;
			
	// single line
	if (from.line == to.line) {

		const u64 line = std::clamp<u64>(from.line, 0u, self->glyphRuns.size() - 1);
		const GlyphRun &glyphRun = self->glyphRuns[line];
		
		float offsetFrom, offsetTo;
		glyphRun.MeasureOffsetRange(from.column, to.column, &offsetFrom, &offsetTo);
		
		const float offsetY = y + (settings.fontEditor.lineHeight * line);
		
		funcAction(offsetY, x + offsetFrom, x + offsetTo);
				
	// multi-line
	} else {
		
		// handle first affected line
		{
			const GlyphRun& glyphRun = self->glyphRuns[from.line];

			const float offsetFrom = x + glyphRun.MeasureOffset(from.column);
			const float offsetTo   = x + std::max(glyphRun.width, 2.0f);
			const float offsetY    = y + (settings.fontEditor.lineHeight * from.line);

			funcAction(offsetY, offsetFrom, offsetTo);
		}

		// handle all lines in between
		for (u64 i = from.line + 1u; i < to.line; i++) {
				
			const GlyphRun& run = self->glyphRuns[i];
			
			const float offsetFrom = x + 0.0f;
			const float offsetTo   = x + std::max(run.width, 2.0f);
			const float offsetY    = y + (settings.fontEditor.lineHeight * i);
			
			funcAction(offsetY, offsetFrom, offsetTo);
		}

		// handle last line
		{
			const GlyphRun& glyphRun = self->glyphRuns[to.line];

			const float offsetFrom = x + 0.f;
			const float offsetTo   = x + std::max(glyphRun.MeasureOffset(to.column), 2.0f);
			const float offsetY    = y + (settings.fontEditor.lineHeight * to.line);

			funcAction(offsetY, offsetFrom, offsetTo);
		}
	}
}

static void HighlightTextRange(f32 y, f32 from, f32 to) {
	deviceContext->FillRoundedRectangle(
		D2D1_ROUNDED_RECT {
			.rect = D2D_RECT_F {
				.left   = from,
				.top    = y,
				.right  = to,
				.bottom = y + settings.fontEditor.lineHeight },
			.radiusX = 2.f,
			.radiusY = 2.f },
		brush);
};

static void DrawScrollbarMarker(Editor* self, ID2D1DeviceContext* deviceContext, u64 line, f32 stroke, ID2D1SolidColorBrush* brush) {
	
	const f32 scrollbarPosY = self->area.top + (line * settings.fontEditor.lineHeight) * self->scrollarea.GetRatio();
	
	deviceContext->DrawLine(
		D2D1_POINT_2F {
			.x = self->area.right - SCROLLBAR_WIDTH_WIDE,
			.y = scrollbarPosY},
		D2D1_POINT_2F {
			.x = self->area.right,
			.y = scrollbarPosY},
		brush,
		stroke);
}

static void DrawDiagnosticsTooltip(Editor* self, ID2D1DeviceContext* deviceContext, const EditorDiagnostics::Record& record, bool isScrollbarTooltip) {
	
	//
	// shape the text
	//
	GlyphRun runCode {};
	GlyphRunMultiline runMessage {};
	
	runCode.Shape(record.code, settings.fontEditor);
	runMessage.Shape(record.message, settings.fontUi);
	
	//
	// measure the width and height
	//
	f32 width = PADDING_X2 + std::max(
		runCode.width + settings.fontEditor.lineHeight + PADDING,
		runMessage.GetWidth());
		
	if (isScrollbarTooltip)
		width = std::max(width, RectWidth(self->area) * 0.3f);
		
	f32 height = PADDING_X2 + settings.fontEditor.lineHeight
			   + PADDING_X2 + (settings.fontUi.lineHeight * runMessage.LineCount());
			   
	if (isScrollbarTooltip)
		height += PADDING_X2 + (settings.fontEditor.lineHeight * 4);		
	
	D2D1_POINT_2F position;
	
	if (!isScrollbarTooltip) {	
		const D2D1_POINT_2F curPos = self->GetCaretLocation();
		position = D2D_POINT_2F {
			.x = curPos.x,
			.y = curPos.y + settings.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y };
	
	} else {		
		position = D2D_POINT_2F {
			.x = self->area.right - SCROLLBAR_WIDTH_WIDE - width - 15.0f,
			.y = self->area.top + (record.from.line * settings.fontEditor.lineHeight * self->scrollarea.GetRatio()) - (height / 2.0f)};
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
				position.y,
		    	width,
		    	settings.fontEditor.lineHeight + PADDING_X2),
			settings.GetBrushUiBackground());
	
		deviceContext->DrawBitmap(
			*Diagnostics::SEVERITY_ICONS[record.severity],
			MakeRect(
				position.x + PADDING,
				position.y + PADDING,
			    settings.fontEditor.lineHeight,
			    settings.fontEditor.lineHeight));
				
		runCode.Draw(deviceContext,
			position.x + PADDING_X2 + settings.fontEditor.lineHeight,
			position.y + PADDING,
			settings.fontEditor,
			settings.GetBrushUiText());
	}
	
	//
	// draw seperator
	//
	{
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = position.x,
				.y = position.y + PADDING_X2 + settings.fontEditor.lineHeight},
			D2D1_POINT_2F {
				.x = position.x + width,
				.y = position.y + PADDING_X2 + settings.fontEditor.lineHeight},
			Diagnostics::GetServerityBrush(record.severity));
	}
	
	//
	// draw message
	//
	{
		runMessage.Draw(deviceContext,
			position.x + PADDING,
			position.y + PADDING_X3 + settings.fontEditor.lineHeight,
			settings.fontUi,
			settings.GetBrushUiText());
	}
		
	//
	// draw context
	//
	if (isScrollbarTooltip) {
	
		const f32 contextStartY = position.y + PADDING_X4
			+ settings.fontEditor.lineHeight
	 		+ (settings.fontUi.lineHeight * runMessage.LineCount());
		
		// draw 2nd seperator
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = position.x,
				.y = contextStartY},
			D2D1_POINT_2F {
				.x = position.x + width,
				.y = contextStartY},
			settings.GetBrushUiBackground());
	
		const s64 sFrom = static_cast<s64>(record.from.line - 1);
		const s64 sTo   = static_cast<s64>(record.from.line + 2);
		for (s64 i = sFrom; i <= sTo; i++) {
			if (i < 0 || i >= static_cast<s64>(self->glyphRuns.size())) continue;
			
			self->glyphRuns[i].Draw(
				deviceContext,
				position.x + PADDING,
				contextStartY + (settings.fontEditor.lineHeight * (i - sFrom)),
				settings.fontEditor,
				settings.GetBrushEditorText());
		}
				
		// draw underline in context
		if (record.from.line == record.to.line) {
			
			const GlyphRun& glyphRun = self->glyphRuns[record.from.line];
			
			float offsetFrom, offsetTo;
			glyphRun.MeasureOffsetRange(record.from.column, record.to.column, &offsetFrom, &offsetTo);
						
			deviceContext->DrawLine(
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetFrom,
					.y = contextStartY + settings.fontEditor.lineHeight + settings.fontEditor.underlineOffset},
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetTo,
					.y = contextStartY + settings.fontEditor.lineHeight + settings.fontEditor.underlineOffset},
				Diagnostics::GetServerityBrush(record.severity));
		}
	}	
}

static TextPosition Hittest(Editor* self, f32 x, f32 y) {
	
	const float absoluteX = mouse.x + self->scrollarea.vpX - self->area.left - GetLineNumberWidth();
	const float absoluteY = mouse.y + self->scrollarea.vpY - self->area.top;
	ASSERT(absoluteY >= .0f);
	
	const u64 line = std::clamp(
		static_cast<u64>(absoluteY / settings.fontEditor.lineHeight),
		0ull,
		self->textController.buffer.GetMaxLine());

	const GlyphRun& run = self->glyphRuns[line];
	const u64 column = run.HitTest(absoluteX);
	
	return TextPosition {
		.line = line,
		.column = column};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnClickScrollArea(void* ud, u64 i) {	
	auto self = static_cast<Editor*>(ud);
	
	const std::scoped_lock lock {self->editorDiagnostics.mutex};
	// in the time between Editor::OnUpdate() and the Click-Execution
	// the diagnostics may have been updated, so it is not guaranteed
	// that "i" is valid.
	if (i >= self->editorDiagnostics.RecordCount()) return;
	
	// @IMPROVE even if i is valid - it might an entirly different
	// record from what the user clicked
	
	
	const EditorDiagnostics::Record& record = self->editorDiagnostics.records[i];
	self->ScrollToLine(record.from.line);
}

void Editor::OnUpdate() {

	//
	// reshape glyphs
	//
	{
		scrollarea.totalSize.height = glyphRuns.size() * settings.fontEditor.lineHeight;
		
		const bool hovered = mouse.Hittest(area, this, nullptr);
		if (hovered) {
			if (mouse.event == Mouse::Event_Down) {
				mouse.StartDragging();
				
				const TextPosition mouseTextPosition = Hittest(this, mouse.x, mouse.y);
				textController.SetCaretPosition(mouseTextPosition);
			
			} else if (mouse.isDragging) {
				ASSERT(textController.carets.size() == 1u);
				
				const TextPosition mouseTextPosition = Hittest(this, mouse.x, mouse.y);
				TextController::Caret& caret = textController.carets.front();
				
				if (!caret.hasSelection && mouseTextPosition != caret.position) {
					caret.selection = caret.position;
					caret.hasSelection = true;
				}
				
				caret.position = mouseTextPosition;
			}
		}
	}
	
	//
	// fill background
	//
	deviceContext->FillRectangle(area, settings.GetBrushEditorBackground());

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
	// get visible line numbers
	//	
	u64 firstVisible, lastVisible;
	GetVisibleLines(&firstVisible, &lastVisible);

	
	//
	// draw line numbers
	//
	{
		u16 digitGlyphIndicies[10];

		// get the glyph indicies for every possible digit
		{
			const auto digitChars = reinterpret_cast<const u32*>(U"0123456789");
			
			const HRESULT hr = settings.fontEditor.fontFace->GetGlyphIndices(digitChars, 10, digitGlyphIndicies);
			if (hr != S_OK) {
				LogError("failed render line numbers. GetGlyphIndices() failed. HRESULT: %", FHr(hr));
				return;
			}
		}
		
		deviceContext->PushAxisAlignedClip(area, D2D1_ANTIALIAS_MODE_ALIASED);
		DEFER(deviceContext->PopAxisAlignedClip());

		// render all line numbers
		for (u64 i = firstVisible; i <= lastVisible; i++) {
	
			u16 glyphsToRender[LINENUMBERS_MAX_DIGITS];
			std::fill_n(glyphsToRender, LINENUMBERS_MAX_DIGITS, settings.fontEditor.glyphIndexSpace);

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
				glyphRun.fontFace = settings.fontEditor.fontFace;
				glyphRun.fontEmSize = settings.fontEditor.size;
				glyphRun.glyphCount = LINENUMBERS_MAX_DIGITS;
				glyphRun.glyphIndices = glyphsToRender;
				
				ID2D1SolidColorBrush* brush;
				for (const TextController::Caret& caret : textController.carets) {
					if (caret.position.line == i) {
						brush = settings.GetBrushEditorText();
						goto draw;
					}
				}
				brush = settings.GetBrushUiText(false);
			
			draw:
				deviceContext->DrawGlyphRun(
					D2D_POINT_2F {
						.x = area.left,
						.y = area.top + (settings.fontEditor.lineHeight * i) - scrollarea.vpY + settings.fontEditor.baselineOffset },
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
						
			for (u64 i = firstVisible; i <= lastVisible; i++) {
				const GlyphRun& glyphRun = glyphRuns[i];
				
				const f32 offsetY = (i * settings.fontEditor.lineHeight);

				glyphRun.Draw(renderTargetText, 0.0f, offsetY, settings.fontEditor, alphaMaskBrush);
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
			renderTargetColor->SetTransform(scrollTransform);
			renderTargetColor->Clear(settings.colors.editorText.ToD2D());

			if (language) {
				renderTargetColor->Clear(settings.colors.editorText.ToD2D());
				
				if (language->syntaxHighlighter)
					language->syntaxHighlighter->Highlight(this, renderTargetColor, firstVisible, lastVisible);
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
			.width  = settings.fontEditor.GetSpaceAdvance(),
			.height = settings.fontEditor.lineHeight};
		
		for (const TextController::Caret& caret : textController.carets) {
			
			// draw selection
			if (TextPosition selFrom, selTo; caret.GetSelection(&selFrom, &selTo)) {
				GetBrush(settings.colors.selection);
				IterateGlyphRange(this, selFrom, selTo, HighlightTextRange);
			}
			
			// draw caret
			const D2D_RECT_F caretRect = MakeRect(
				TranslateTextPosition(this, caret.position),
				caretSize);
				
			ID2D1SolidColorBrush* brushCaret = settings.GetBrushEditorText();			
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
			
			ID2D1SolidColorBrush* brushEditCaret = settings.GetBrushEditorMultiCaretEdit();
			const D2D_RECT_F caretRect = MakeRect(
				TranslateTextPosition(this, textController.editCaretsPosition),
				caretSize);
			
			if (!std::isnan(fillingOpacity)) {
				brushEditCaret->SetOpacity(fillingOpacity);
				deviceContext->FillRectangle(caretRect, settings.GetBrushEditorMultiCaretEdit());
				brushEditCaret->SetOpacity(1.0f);
			}
			
			for (const TextController::Caret& caret : textController.carets) {
				if (caret.position == textController.editCaretsPosition) {
					brushEditCaret = settings.GetBrushEditorText();
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
				
				const GlyphRun& run = glyphRuns[result.from.line];
				
				deviceContext->DrawRectangle(
					D2D1_RECT_F {
						.left   = offsetX + run.MeasureOffset(result.from.column),
						.top    = offsetY + result.from.line * settings.fontEditor.lineHeight,
						.right  = offsetX + run.MeasureOffset(result.to.column),
						.bottom = offsetY + (result.from.line + 1) * settings.fontEditor.lineHeight },
					settings.GetBrushEditorText());
			}
		}
	}

	//
	// insert-animation
	//
	if (insertAnimationRunning) {
		
		ID2D1SolidColorBrush* brushInsertAnim = GetBrush(settings.colors.selection);
		brushInsertAnim->SetOpacity(insertAnimationOpacity);
		DEFER(brushInsertAnim->SetOpacity(1.0f));
		
		for (const InsertAnimationData& insertData : insertAnimationData)
			IterateGlyphRange(this, insertData.from, insertData.to, HighlightTextRange);
		
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
		const std::scoped_lock lock {editorDiagnostics.mutex};
		
		u64 recordToShowDetailsFor  = U64_MAX;
		for (u64 i = 0u; i < editorDiagnostics.RecordCount(); i++) {
			const EditorDiagnostics::Record& record = editorDiagnostics.records[i];
			
			// change the brush color
			GetBrush(Diagnostics::SEVERITY_COLORS[record.severity]);
			
			// draw underlines
			IterateGlyphRange(this, record.from, record.to, [] (f32 offsetY, f32 offsetFrom, f32 offsetTo) {
				auto clr = brush->GetColor();
				if (clr.r == 1.0f && clr.g == 1.0f && clr.b == 1.0f)
					int stop = 0;
				
				deviceContext->DrawLine(
					D2D_POINT_2F {
						.x = offsetFrom,
						.y = offsetY + settings.fontEditor.underlineOffset},
					D2D_POINT_2F { 
						.x = offsetTo,
						.y = offsetY + settings.fontEditor.underlineOffset},
					brush,
					2.0f,
					nullptr);
			});
			
			// draw error details-window
			const bool showErrorDetails = !editorCaretAttached
			                           && !textController.HasSelection()
			                           && !textController.isEditCaretsMode
			                           && (textController.carets.size() == 1u)
			                           && record.from <= textController.carets.front().position
			                           && record.to > textController.carets.front().position;
			                              
			if (showErrorDetails) recordToShowDetailsFor = i;
			
			// draw scrollbar marker			
			DrawScrollbarMarker(this, deviceContext, record.from.line, 1.0f, brush);
		}
		
		if (recordToShowDetailsFor != U64_MAX)
			DrawDiagnosticsTooltip(this, deviceContext, editorDiagnostics[recordToShowDetailsFor], false);
	}
	
	//
	// handle scrollbar interaction
	//
	{
		if (mouse.x > area.right - SCROLLBAR_WIDTH_WIDE && mouse.x < area.right) {
			const std::scoped_lock lock {editorDiagnostics.mutex};
			
			f32 recordClosestToMouseDistance = F32_MAX;
			u64 recordClosestToMouse = U64_MAX;
	
			for (u64 i = 0; i < editorDiagnostics.RecordCount(); i++) {
				const EditorDiagnostics::Record& record = editorDiagnostics.records[i];
				
				const f32 scrollbarPosY = area.top + (record.from.line * settings.fontEditor.lineHeight) * scrollarea.GetRatio();
				
				const f32 distanceToMouse = std::abs(mouse.y - scrollbarPosY);
				
				if (distanceToMouse < settings.scrollbarMarkerHoverDistance && distanceToMouse < recordClosestToMouseDistance) {
					recordClosestToMouse = i;
					recordClosestToMouseDistance = distanceToMouse;
				}
			}
			
			if (recordClosestToMouse != U64_MAX) {
				const EditorDiagnostics::Record& record = editorDiagnostics[recordClosestToMouse];
				
				if (!mouse.isDragging) {
					mouse.hotElementNext = Mouse::Element {this, OnClickScrollArea};
					mouse.onClickArg = recordClosestToMouse;
					mouse.dragArg = 0.0f;
				}
				
				DrawDiagnosticsTooltip(this, deviceContext, record, true);
				DrawScrollbarMarker(this, deviceContext, record.from.line, 2.0f, Diagnostics::GetServerityBrush(record.severity));
			}
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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void Editor::OnResize(D2D1_RECT_F newArea) {
	this->area = newArea;
	this->scrollarea.OnResize(newArea);
}

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
	scrollarea.ScrollVertical(distance * settings.fontEditor.lineHeight * 5);
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
			
	if (event == settings.keybinds.openSearch || event == settings.keybinds.openSearchAndReplace) {
	
		const bool showReplace = event == settings.keybinds.openSearchAndReplace;
		
		if (auto search = dynamic_cast<EditorSearch*>(toolWindow)) {
			search->ToggleReplaceTextbox(showReplace);

		} else {
			delete toolWindow;
			
			toolWindow = EditorSearch::Make(this, showReplace);
			if (!toolWindow)
				LogError("EditorSearch::Make() failed");
		}
		return;
			
	} else if (event == settings.keybinds.openGotoLine) {
		
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
	
	} else if (event == settings.keybinds.showGotoLocation) {
		if (!language) return;
			
		if (editorCaretAttached) {
			editorCaretAttached->RemoveReference();
			editorCaretAttached = nullptr;
		}
		
    	editorCaretAttached = EditorSelectGotoType::Make(this);
    	return;
	
	} else if (event == settings.keybinds.showSignatureHelp) {
		if (!language) return;	
		
		if (editorCaretAttached) {
			editorCaretAttached->RemoveReference();
			editorCaretAttached = nullptr;
		}
		
    	editorCaretAttached = language->GetSignatureHelp(this);
    	return;
    	
	} else if (event == settings.keybinds.showAutocomplete) {
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
	
	} else if (event == settings.keybinds.saveFile) {
		SaveFile();
		return;
	
	} else if (event == settings.keybinds.scrollUp) {
		scrollarea.vpY -= (settings.fontEditor.lineHeight * 2);
		if (scrollarea.vpY < 0.0f)
			scrollarea.vpY = 0.0f;
		return;
	
	} else if (event == settings.keybinds.scrollDown) {
		scrollarea.vpY += (settings.fontEditor.lineHeight * 2);
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
	if (isDirty) {
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
	
	ts_parser_delete(tsParser);
	ts_tree_delete(tsTree);
}