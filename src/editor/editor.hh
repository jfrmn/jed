#pragma once
#include "basic.hh"

#include "text/text-position.hh"
#include "text/text-controller.hh"

#include "ui/scrollarea.hh"
#include "graphics/glyph-run.hh"

#include "editor/editor-diagnostics.hh"

#include <string>

struct Event;
struct Command;

struct Language;
struct TextChange;
struct FileChangeRecord;

struct EditorCaretAttached;
struct EditorToolWindow;

struct TSParser;
struct TSTree;

struct Editor {
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

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

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	D2D_RECT_F area = {};
	
	std::string path = {};
	FILETIME lastWriteTime = {};
	bool fileRemoved = false;
	bool isDirty = false;
	
	TextController textController = {};
	EditorDiagnostics editorDiagnostics = {};
	
	EditorCaretAttached* editorCaretAttached = nullptr;
	EditorToolWindow*    toolWindow = nullptr;

	TextDocumentIdentifier textDocumentIdentifier = {};	
	Language* language = nullptr;

	std::vector<GlyphRun> glyphRuns = {};

	f32 scrollTargetPosition = .0f;
	f32 scrollSpeed          = .0f;
	Scrollarea scrollarea    = {};
	
	bool insertAnimationRunning                          = false;
	std::vector<InsertAnimationData> insertAnimationData = {};
	f32 insertAnimationOpacity                           = 0.0f;
	
	f32 cursorBlinkValue = 0.0f;
	u64 cursorBlickStopTick = 0u;
	
	TSParser* tsParser = nullptr;
	TSTree* tsTree = nullptr;
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	bool Init();
	~Editor() noexcept;
	
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
	
	void Update();
	
	void OnResize(const D2D_RECT_F& newArea);
	void OnMouseWheel(f32 distance);
	void OnFileChange(const FileChangeRecord* fileChangeRecord);
	bool HandleEvent(const Event& event, const Command& command);	
};
