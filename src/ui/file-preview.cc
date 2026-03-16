#include "file-preview.hh"
#include "globals.hh"
#include "settings.hh"

#include "ui/constants.h"
#include "graphics/effects.hh"

#include "util/logging.hh"
#include "util/rect-util.hh"

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
		FormatToString(buffer, "Error: %", FLastErr(lastErr));
		
	LogWarning("file preview: %", *buffer);
};

bool FilePreview::Load(const LoadArgs& args) {

	LogDetail("loading file preview for '%' in mode %", args.path, args.mode);

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
				if (lastErr != ERROR_HANDLE_EOF) {
					SetReadError(this, lastErr);
					return false;
				
				} else {
					break; // end of file reached
				}
			}
			
			textBuffer.Insert(
				TextPosition {textBuffer.GetMaxLine(), textBuffer.lines.back().length},
				std::string_view {chunk, numOfBytesRead},
				nullptr);
		}
	}
	
	u64 from = 0u, to = 0u;
	
	//
	// get displayed lines
	//
	if (args.mode == LoadMode_FirstFewLine) {
		from = 0u;
		to = std::min<u64>(args.lineCount - 1u, textBuffer.GetMaxLine());
		
	} else if (args.mode == LoadMode_TargetLine) {
		
		const s64 sTargetLine = static_cast<s64>(args.targetLine);
		from = std::max<s64>(sTargetLine - TARGET_LINE_LINES_BEFORE, 0);
		to   = std::min<s64>(args.targetLine + TARGET_LINE_LINES_AFTER, textBuffer.GetMaxLine());
		
	} else if (args.mode == LoadMode_LineRange) {
		ASSERT(args.lineFrom <= args.lineTo);
		
		from = args.lineFrom;
		if (from > textBuffer.GetMaxLine())
			from = std::max<s64>(0, static_cast<s64>(textBuffer.GetMaxLine()) - 5);
			
		to = std::max<u64>(0u, textBuffer.GetMaxLine());
	
	} else {
		ASSERT_UNREACHABLE;
	}
	
	//
	// shape lines
	//
	{
		lines.clear();
		width = 0.0f;
		
		for (u64 i = from; i <= to; i++) {
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
	if (args.hasSelection) {
		
		const TextPosition minPos {
			.line = from,
			.column = 0u};
			
		const TextPosition maxPos {
			.line = to,
			.column = textBuffer.GetLineAt(to).length};
		
		TextPosition clampedSelectionFrom = std::clamp(args.selectionFrom, minPos, maxPos);
		TextPosition clampedSelectionTo = std::clamp(args.selectionTo, minPos, maxPos);
		
		this->selectionFrom = TextPosition {
			.line = to - clampedSelectionFrom.line,
			.column = clampedSelectionFrom.column};
		
		this->selectionTo = TextPosition {
			.line = to - clampedSelectionTo.line,
			.column = clampedSelectionTo.column};
	
		this->hasSelection = true;		
		
		ASSERT(selectionFrom.line <= selectionTo.line);
	
	} else {
		this->selectionFrom = TextPosition {};
		this->selectionTo = TextPosition {};
		this->hasSelection = false;
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FilePreview::OnUpdate() {
	
	const D2D_RECT_F area = GetArea();
	BlurArea(deviceContext, area);
	
	deviceContext->PushAxisAlignedClip(area, D2D1_ANTIALIAS_MODE_ALIASED);
	
	for (u64 i = 0u; i < lines.size(); i++) {
		const GlyphRun& run = lines[i];
		run.Draw(deviceContext, x + PADDING, y + (i * settings.fontEditor.lineHeight) + PADDING, settings.fontEditor, settings.GetBrushEditorText());
	}
	
	deviceContext->PopAxisAlignedClip();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
D2D_RECT_F FilePreview::GetArea() const {
	const f32 height = lines.size() * settings.fontEditor.lineHeight + PADDING_X2;
	return MakeRect(x, y, width, height);
}
