#pragma once
#include "events.hh"
#include "console.hh"
#include "ui/window.hh"
#include "ui/status-bar.hh"
#include "graphics/glyph-run.hh"

#include <vector>
#include <string>

struct Prompt;
struct SearchBar;
struct Explorer;
struct TextPosition;
struct Console;

// @DUMMY
struct ParameterConfigurator;

struct MainWindow : public Window {

	//-----------------------------------------------------
	// types

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

	SearchBar* searchBar = nullptr;
	Explorer* explorer = nullptr;
	Console console = {};
	StatusBar statusBar = {};
	
	std::vector<Tab>   tabs   = {};
	std::vector<Panel> panels = {};
	
	u64 focusedPanelIndex   = 0u;
	u64 hoveredTabIndex     = U64_MAX;
	bool closeButtonHovered = false;
	
	bool recievedQuit = false;

	//-----------------------------------------------------
	// functions

	bool Create();

	bool Init();
	void Shutdown();
	
	Editor* OpenEditor(std::string path, OpenBehavior openBehavior = OpenBehavior_Default, bool* wasAlreadyOpen = nullptr);
	void RenameEditor(const Editor* editor, std::string_view newName);
	
	void OnUpdate();
	
	virtual void OnFileChanged(FileChangedEvent* fileChangedEvent) override;
	virtual bool OnMouseWheel(f32 wheel) override;
	virtual void OnKeyDown(KeyEvent event) override;
	virtual void OnChar(const char* data, u64 len) override;
	virtual void OnResize(f32 newWidth, f32 newHeight) override;
	virtual bool OnClose() override;
};

// Helper function to make open behavior consisten across different feature (explore, file-picker, etc.)
MainWindow::OpenBehavior OpenBehaviorFromModifiers(KeyEvent event);

extern MainWindow mainWindow;