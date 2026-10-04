#include "file-preview.hh"

#include "settings.hh"

#include "ui/constants.h"
#include "graphics.hh"

#include "logging.hh"
#include "util.hh"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr u32 READ_CHUNK_SIZE = 1024;
static constexpr u64 TARGET_LINE_LINES_BEFORE = 2;
static constexpr u64 TARGET_LINE_LINES_AFTER  = 7;

static constexpr f32 MAX_WIDTH = 500.0f;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilePreview::Init() {
	textBuffer.Init();
	color = settings.colors.dropShadow;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void SetReadError(FilePreview* self, u32 lastErr) {
	self->hasError = true;
	
	std::string* buffer = self->textBuffer.Clear();
	if (lastErr == ERROR_FILE_NOT_FOUND)
		buffer->assign("File not found");
	else if (lastErr == ERROR_LOCK_VIOLATION)
		buffer->assign("File is locked");
	else
		FormatString(buffer, "Error: %s", StrLastErr(lastErr));
		
	LogWarning("file preview: %s", buffer->c_str());
};

bool FilePreview::Load(const LoadArgs& args) {

	LogTrace("loading file preview for '%.*s' in mode %u", SIZE_AND_DATA(args.path), args.mode);

	// reset prev. error
	this->hasError = false;
	
	//
	// open file
	//
	HANDLE hFile = CreateFileA(args.path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		SetReadError(this, GetLastError());
		return false;
	}
	DEFER(CloseHandle(hFile));	
	
	//
	// read initial chunk
	//
	{
		std::string* buffer = textBuffer.Clear();
		
		buffer->resize(READ_CHUNK_SIZE);
		
		DWORD numOfBytesRead = 0;	
		const bool ok = ReadFile(hFile, buffer->data(), READ_CHUNK_SIZE, &numOfBytesRead, nullptr);
		if (!ok) {
			const u32 lastErr = GetLastError();
			if (lastErr != ERROR_HANDLE_EOF) {
				SetReadError(this, lastErr);
				return false;
			}
		}
		
		buffer->resize(numOfBytesRead);
		
		textBuffer.RecreateLines();
	}
	
	//
	// determine how many lines we want
	//
	u64 desiredLineCount = 0;
	if (args.mode == LoadMode_FirstFewLine)
		desiredLineCount = args.lineCount;
	else if (args.mode == LoadMode_TargetLine)
		desiredLineCount = args.targetLine + 7u;
	else if (args.mode == LoadMode_LineRange)
		desiredLineCount = args.lineTo;
	else ASSERT_UNREACHABLE;
	
	//
	// read more until we reached the desired line 
	//
	{
		char chunk[READ_CHUNK_SIZE] {0};
	
		while (textBuffer.LineCount() <= desiredLineCount) {
			
			memset(chunk, 0, sizeof(char) * READ_CHUNK_SIZE);
			
			DWORD numOfBytesRead = 0;
			const bool ok = ReadFile(hFile, chunk, READ_CHUNK_SIZE, &numOfBytesRead, nullptr);
			
			if (!ok) {
				const u32 lastErr = GetLastError();
				if (lastErr == ERROR_HANDLE_EOF) break;
				
				SetReadError(this, lastErr);
				return false;
			}
			
			textBuffer.Insert(
				TextPosition {textBuffer.GetMaxLine(), textBuffer.lines.back().length},
				std::string_view {chunk, numOfBytesRead},
				nullptr);
		}
	}
	
	u64 fromLine = 0u, toLine = 0u;
	
	//
	// get displayed lines
	//
	if (args.mode == LoadMode_FirstFewLine) {
		fromLine = 0u;
		toLine   = std::min<u64>(args.lineCount - 1u, textBuffer.GetMaxLine());
		
	} else if (args.mode == LoadMode_TargetLine) {
		
		const s64 sTargetLine = static_cast<s64>(args.targetLine);
		fromLine = std::max<s64>(sTargetLine - TARGET_LINE_LINES_BEFORE, 0);
		toLine   = std::min<s64>(args.targetLine + TARGET_LINE_LINES_AFTER, textBuffer.GetMaxLine());
		
	} else if (args.mode == LoadMode_LineRange) {
		ASSERT(args.lineFrom <= args.lineTo);
		
		fromLine = args.lineFrom;
		if (fromLine > textBuffer.GetMaxLine())
			fromLine = std::max<s64>(0, static_cast<s64>(textBuffer.GetMaxLine()) - 5);
			
		toLine = std::max<u64>(0u, textBuffer.GetMaxLine());
	
	} else {
		ASSERT_UNREACHABLE;
	}
	
	//
	// shape lines
	//
	{
		lines.clear();
		width = 0.0f;
		
		for (u64 i = fromLine; i <= toLine; i++) {
			GlyphRun& run = lines.emplace_back();
			const TextBuffer::Line& line = textBuffer.GetLineAt(i);
			
			run.Shape(line.GetText(), settings.fontEditor);
			
			const f32 runWidth = run.width + PADDING_X2;
			if (width < runWidth)
				width = runWidth;
		}
		
		width = std::min(width, MAX_WIDTH);
	}
	
	//
	// handle selection
	//
	if (args.highlightMode == HighlightMode_None) {
		// do nothing
		
	} else if (args.highlightMode == HighlightMode_Selection) {
		
		const TextPosition minPos {
			.line = fromLine,
			.character = 0u};
			
		const TextPosition maxPos {
			.line = toLine,
			.character = textBuffer.GetLineAt(toLine).length};
		
		const TextPosition clampedSelectionFrom = std::clamp(args.selectionFrom, minPos, maxPos);
		const TextPosition clampedSelectionTo   = std::clamp(args.selectionTo,   minPos, maxPos);
	
		this->highlightMode = HighlightMode_Selection;
		
		this->highlightFrom = TextPosition {
			.line = clampedSelectionFrom.line - fromLine,
			.character = clampedSelectionFrom.character};
		
		this->highlightTo = TextPosition {
			.line = clampedSelectionFrom.line - fromLine,
			.character = clampedSelectionTo.character};
				
		ASSERT(highlightFrom.line <= highlightTo.line);
	
	
	} else if ((args.highlightMode == HighlightMode_Underline) &&
		       (args.underlinedLine >= fromLine) &&
		       (args.underlinedLine <= toLine)) {
		
		const u64 adjustedLine = args.underlinedLine - fromLine;
		ASSERT(adjustedLine < lines.size()) // check condition above. might a off by one error
		
		const std::string_view& lineText = textBuffer.GetLineAt(args.underlinedLine).GetText();
		
		u64 startPos = 0u, endPos = 0u;		
		if (const auto it = std::find_if_not(lineText.begin(), lineText.end(), isspace); it != lineText.end()) {
			
			const auto rit = std::find_if_not(lineText.rbegin(), lineText.rend(), isspace);
			ASSERT(rit != lineText.rend());  // we know the line is not empty so that should return something
			
			startPos = static_cast<u64>(std::distance(lineText.begin(), it));
			endPos = static_cast<u64>(lineText.size() - 1u - std::distance(lineText.rbegin(), rit));
					
		} else {
			// line is either empty or blank
			startPos = 0u;
			endPos = std::min(0ull, lineText.size()-1u);
		}
		
		this->highlightMode = HighlightMode_Underline;
		this->highlightFrom = TextPosition {
			.line = adjustedLine,
			.character = startPos};
		this->highlightTo = TextPosition {
			.line = adjustedLine,
			.character = endPos};
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilePreview::OnUpdate() {
	
	// background
	const D2D_RECT_F area = GetArea();
	ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, area);
	if (!background) return;
	DEFER(background->Release());

	DrawGlow(deviceContext, background, area, &color);

	PushLayer(deviceContext, area);
	DEFER(PopLayer(deviceContext));
	
	BlurArea(deviceContext, area, background);
	
	// lines
	for (u64 i = 0u; i < lines.size(); i++) {
		const GlyphRun& run = lines[i];
		run.Draw(deviceContext, x + PADDING, y + (i * settings.fontEditor.lineHeight) + PADDING, settings.fontEditor, settings.colors.UseEditorText());
	}
	
	if (highlightMode != HighlightMode_None) {
		ID2D1SolidColorBrush* brush = UseColor(color);
		
		IterateTextRange(highlightFrom, highlightTo, [&] (u64 line, u64 columnFrom, u64 columnTo) {
			ASSERT(line < lines.size());
			const GlyphRun& run = lines[line];
			
			f32 from = 0.0f, to = 0.0f;
			lines[line].MeasureOffsetRange(columnFrom, columnTo, &from, &to);
					
			if (highlightMode == HighlightMode_Selection) {
				deviceContext->FillRectangle(
					D2D_RECT_F {
						.left   = x + PADDING + from,
						.top    = y + (settings.fontEditor.lineHeight * line),
						.right  = x + PADDING + to,
						.bottom = y + (settings.fontEditor.lineHeight * (line+1u))},
					brush);
			
			} else if (highlightMode == HighlightMode_Underline) {
				const f32 y = this->y + PADDING + (settings.fontEditor.lineHeight * line) + settings.fontEditor.underlineOffset;
				deviceContext->DrawLine(
					D2D_POINT_2F {x + PADDING + from, y},
					D2D_POINT_2F {x + PADDING + to,   y},
					brush);
			
			} else {
				ASSERT_UNREACHABLE;
			}
		});
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
D2D_RECT_F FilePreview::GetArea() const {
	const f32 height = lines.size() * settings.fontEditor.lineHeight + PADDING_X2;
	return MakeRect(x, y, width, height);
}
