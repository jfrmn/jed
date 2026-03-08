#pragma once
#include "basic.hh"
#include "graphics/glyph-run-dwrite.hh"
#include "text/text-position.hh"
#include "text/text-controller.hh"

#include "ui/scrollarea.hh"
#include "util/string-util.hh"

#include "editor/editor-diagnostics.hh"

#include <string>

struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;

struct Language;
struct TextChange;
struct MouseEvent;

struct EditorCaretAttached;
struct EditorToolWindow;

struct  Editor {
	
	//-------------------------------------------
	// types

	struct InsertAnimationData {
		TextPosition from = {};
		TextPosition to = {};
	};

	enum FileResult {
         FileResult_Failure  = 0,
         FileResult_Success  = 1,
		 FileResult_Canceled = 2,
	};
	
	// needed to communicate with lsp-server	
	struct TextDocumentIdentifier {
		std::string uri = {};
		s32 version = 0;
	};

	//-------------------------------------------
	// data
	
	D2D_RECT_F area = {};
	
	std::string path = {};
	bool modified = false;
	
	Encoding encoding = Encoding_Utf8;

	TextController textController = {};
	EditorDiagnostics editorDiagnostics = {};
	
	EditorCaretAttached* editorCaretAttached = nullptr;
	EditorToolWindow*    toolWindow = nullptr;

	TextDocumentIdentifier textDocumentIdentifier = {};	
	Language* language = nullptr;

	std::vector<GlyphRun_DWrite> glyphRuns = {};

	f32 scrollTargetPosition = .0f;
	f32 scrollSpeed          = .0f;
	Scrollarea scrollarea    = {};
	
	bool insertAnimationRunning                          = false;
	std::vector<InsertAnimationData> insertAnimationData = {};
	f32 insertAnimationOpacity                           = 0.0f;
	
	f32 cursorBlinkValue = 0.0f;
	u64 cursorBlickStopTick = 0u;
	
	//-------------------------------------------
	// functions

	bool Init();
	
	FileResult OpenFile(std::string path);
	FileResult CloseFile();
	bool SaveFile();

	void ScrollToLine(u64 line);
	void ProcessTextChange(const TextChange* change);
	D2D_POINT_2F GetCaretLocation() const;
	TextBuffer& GetBuffer();
	void GetVisibleLines(/*out*/ u64* first, /*out*/ u64* last) const;

	void PrepareInsertAnimation(u64 capacity = 0);
	void AddInsertAnimationData(TextPosition from, TextPosition to);
	void StartInsertAnimation();

	void OnUpdate();
	
	void OnChar(const char* str, u64 len);
	void OnMouseWheel(f32 scrollValue);
	void OnKeyDown(KeyEvent event);
	void OnResize(D2D1_RECT_F newArea);
	bool OnClose();
	
	DISALLOW_COPY_AND_ASSING(Editor)
};

