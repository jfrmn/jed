#pragma once
#include "text/text-position.hh"
#include "text/text-buffer.hh"
#include "text/text-change.hh"
#include "util/ring-buffer.h"

#include <vector>

struct Editor;
struct Event;
struct Command;

struct TextController {
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
		
	enum LineEnding {
		 LineEnding_CrLf,
		 LineEnding_Lf,
		 LineEnding_Cr,
		 LineEnding_MAX
	};
	
	struct Caret {
		TextPosition position = {};
		TextPosition selection = {};	
		bool hasSelection = false;

		void ResetSelection();		
		bool GetSelection(/*out*/ TextPosition* from, /*out*/ TextPosition* to) const;
		
		bool operator<(const Caret& other) const;
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------	
	
	Editor* ownerEditor = nullptr;

	TextBuffer buffer = {};
	
	std::vector<Caret> carets = {};
	
	// position of the cursor when in EditMultiCaret mode
	// can't have a selection
	TextPosition editCaretsPosition = {}; 
	bool isEditCaretsMode = false;

	RingBuffer<TextChange> history   = {};
	u64        historyUndoIndex      = U64_MAX;
	TextChange historyUndoTextChange = {};
	
	LineEnding lineEnding        = LineEnding_CrLf;
	std::string_view indentation = {};
	
	// for single char edits (so regular typing) we don't want a history entry for every
	// pressed char. This flag indicates that the previos edit was just one char and we
	// should merge the history record 
	bool previousEditWasSignleChar  = false;

	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	bool InitForTextbox(std::string initialText);
	bool InitForEditor(Editor* owner);

	void Reset();

	bool HasSelection() const;
	bool GetSelection(/*out*/ TextPosition* from, /*out*/ TextPosition* to) const;
	
	void SetCaretPosition(TextPosition pos);
	// toggle caret at the current editCaretsPosition
	// gets called from the editor onClickHandler
	void ToggleCaret();
	
	void SetSelection(TextPosition from, TextPosition to);	
	
	TextChange* NewTextChange();
	
	bool HandleEvent(const Event& event, const Command& command, /*out*/ TextChange** change);
};
