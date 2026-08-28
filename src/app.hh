#pragma once
#include "tool-output.hh"
#include "search-bar.hh"
#include "ui/window.hh"
#include "ui/status-bar.hh"
#include "glyph-run.hh"

#include <vector>
#include <string>

struct Explorer;

struct App {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	struct Tab {
		GlyphRun title = {};
		Editor* editor = nullptr;
		u64 panelIndex = U64_MAX;
	};
	
	struct Panel {		
		Editor* editor = nullptr;
		u64 tabIndex = U64_MAX;
	};

	enum OpenBehavior {
		 // add a tab and reveal the new tab the current panel
		 OpenBehavior_Default = 0,
		 
		 // do not add a tab, change the current tab to the new file
		 OpenBehavior_UpdateCurrent,
         
         // add a tab and reveal the tab in a new panel to the lef/right of the current panel
		 OpenBehavior_NewPanelLeft,
		 OpenBehavior_NewPanelRight,
		 
		 // do not reveal the opened file in any panel, just add the tab
		 OpenBehavior_TabOnly
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	SearchBar* searchBar = nullptr;
	SearchBarFiles searchBarFiles = {};
	SearchBarTools searchBarTools = {};
	SearchBarCommands searchBarCommands  = {};

	Explorer*  explorer = nullptr;
	ToolOutput toolOutput = {};
	StatusBar  statusBar = {};
	
	std::vector<Tab>   tabs   = {};
	std::vector<Panel> panels = {};
	
	u64 focusedPanelIndex   = U64_MAX;
	u64 hoveredTabIndex     = U64_MAX;
	bool closeButtonHovered = false;
	
	bool recievedQuit = false;

	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	bool Init();
	void Shutdown();
	
	Editor* OpenEditor(std::string path, OpenBehavior openBehavior = OpenBehavior_Default, bool* wasAlreadyOpen = nullptr);
	
	Editor* GetFocusedEditor();
	const Editor* GetFocusedEditor() const;
	
	void Update();
	void HandleEvent(const Event& event);
	void PullEvent();
};

// Helper function to make open behavior consisten across different feature (explore, file-picker, etc.)
App::OpenBehavior OpenBehaviorFromModifiers(u32 kmods);

extern App app;