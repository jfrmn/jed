#pragma once
#include "basic.hh"
#include "events.hh"

struct cJSON;

union KeyBindings {
	struct Actions {	
		//
		// window
		//
		KeyEvent showFileSearch;
		KeyEvent showExplorer;
		KeyEvent showToolSearch;
		KeyEvent showConsole;
		KeyEvent focusNextTab;
		KeyEvent focusPrevTab;
		KeyEvent focusNextPanel;
		KeyEvent focusPrevPanel;
		KeyEvent addPanelAfter;
		KeyEvent addPanelBefore;
		KeyEvent swapPanels;
		KeyEvent closePanel;
		KeyEvent closeTab;
		KeyEvent closePanelAndTab;
		
		//
		// editor
		//
		KeyEvent openSearch;
		KeyEvent openSearchAndReplace;
		KeyEvent openGotoLine;
		KeyEvent showSignatureHelp;
		KeyEvent showAutocomplete;
		KeyEvent showGotoLocation;
		KeyEvent saveFile;
		KeyEvent scrollUp;
		KeyEvent scrollDown;
		KeyEvent gotoPrevDiagnostic;
		KeyEvent gotoNextDiagnostic;
		
		//
		// text
		//
		KeyEvent moveToPrevWord;
		KeyEvent moveToNextWord;
		KeyEvent moveToLineStart;
		KeyEvent moveToLineEnd;
		KeyEvent moveToBufferStart;
		KeyEvent moveToBufferEnd;
		KeyEvent movePageUp;
		KeyEvent movePageDown;
		
		KeyEvent selectBackward;
		KeyEvent selectForward;
		KeyEvent selectLineUp;
		KeyEvent selectLineDown;
		KeyEvent selectToPrevWord;
		KeyEvent selectToNextWord;
		KeyEvent selectToLineStart;
		KeyEvent selectToLineEnd;
		KeyEvent selectToBufferStart;
		KeyEvent selectToBufferEnd;
		KeyEvent selectPageUp;
		KeyEvent selectPageDown;
		KeyEvent selectAll;
		KeyEvent selectLine;
		KeyEvent selectInBrackets;
		KeyEvent selectWord;
		
		KeyEvent deletePrevChar;
		KeyEvent deleteNextChar;
		KeyEvent deletePrevWord;
		KeyEvent deleteNextWord;
		KeyEvent deleteLine;
		
		KeyEvent indentLine;
		KeyEvent unindentLine;
		KeyEvent insertTab;
		KeyEvent duplicateLine;
		KeyEvent undo;
		KeyEvent redo;
		KeyEvent cut;
		KeyEvent copy;
		KeyEvent paste;
		KeyEvent cutLines;
		KeyEvent lineComment;
		KeyEvent lineUncomment;
		KeyEvent blockComment;
		KeyEvent blockUncomment;
		
		KeyEvent addCaretAbove;
		KeyEvent addCaretBelow;
		KeyEvent editCarets;	
				
		//
		// explorer
		//
		KeyEvent explorerShellExecute;
		KeyEvent explorerOpenInWindowsExplorer;
		KeyEvent explorerNewFile;
		KeyEvent explorerNewFolder;
		KeyEvent explorerRename;
		
		//
		// console
		//
		KeyEvent consoleTerminateProcess;
		KeyEvent consoleCopy;
	};
	
	static constexpr u64 NUM_ACTIONS = (sizeof(Actions) / sizeof(KeyEvent));
	
	Actions actions = {};
	KeyEvent array[NUM_ACTIONS];
	
	bool Init(const cJSON* json);
	
	static_assert(sizeof(actions) == sizeof(array));	
};

extern KeyBindings keybinds;
