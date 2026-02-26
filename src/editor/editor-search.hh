#pragma once
#include "editor/editor-toolwindow.hh"
#include "ui/text-box.hh"

struct Editor;
struct TextBuffer;
struct MouseEvent;
struct KeyEvent;

struct ID2D1DeviceContext;

struct EditorSearch : public EditorToolWindow {

	//-----------------------------------------------------
	// types

	struct SearchResult {
		TextPosition from = {};
		TextPosition to   = {};
	};

	struct ThreadData {
		EditorSearch* self = nullptr;

		const TextBuffer* textBuffer      = nullptr;
		std::string searchTerm            = {};
		std::vector<SearchResult> results = {};

		std::atomic_bool isCanceled = false;
		std::atomic_bool isComplete = false;
	};
	

	using HThread = void*;

	//-----------------------------------------------------
	// data

	Editor*  owner = nullptr;
	TextBox  textboxSearch  = {};
	TextBox  textboxReplace = {};
	TextBox* focusedTextbox = nullptr;

	D2D_RECT_F area = {};

	GlyphRun glyphRunHeadline = {};
	
	bool isReplaceTextboxVisible = false;
	bool isResultListButtonHovered = false;
	bool isResultListVisible = false;
	
	bool searchIsDirty = false;
	ThreadData* threadData = nullptr;
	HThread hThread = NULL;
	
	//-----------------------------------------------------
	// functions

	static EditorSearch* Make(Editor* editor, bool showReplace);

	bool IsSearchComplete() const;
	void ToggleReplaceTextbox(bool show);
	
	virtual bool IsSearch() const override;

	virtual void OnUpdate() override;
	
	virtual bool OnKeyDown(KeyEvent event) override;
	virtual bool OnChar(const char* data, u64 len) override;

	static void OnToggleCaseSensitivity(void* userdata) {}
	static void OnToggleWholeWord(void* userdata) {}
	static void OnToggleEscapeSequences(void* userdata) {}

	DISALLOW_COPY_AND_ASSING(EditorSearch);
};