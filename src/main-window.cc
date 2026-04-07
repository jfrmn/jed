#include "main-window.hh"
#include "basic.hh"
#include "globals.hh"
#include "settings.hh"

#include "file-search-bar.hh"
#include "explorer.hh"
#include "console.hh"
#include "commands/tool-search-bar.hh"

#include "util/logging.hh"
#include "util/file-util.hh"
#include "util/rect-util.hh"

#include "graphics/effects.hh"
#include "editor/editor.hh"
#include "ui/constants.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
// #include <d2d1_1.h>
#include <d2d1_3.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Creation and Shutdown
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool MainWindow::Create() {
	const bool ok = Window::Create(
		Window::CreateParams {
			.className = "MAINWND",
			.title = "slick-edit",
			.width = 1920,
			.height = 1080,
			.hwndParent = NULL});
	
	if (ok) {
		::deviceContext = this->deviceContext;
		::deviceContext->AddRef();
		return true;
	} else {
		LogError("creating window failed");
		return false;
	}
}

bool MainWindow::Init() {
	if (!statusBar.Init()) {
		LogError("init status bar failed");
		return false;
	}
	
	if (!console.Init()) {
		LogError("init status bar failed");
		return false;
	}
	
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	return true;
}

void MainWindow::Shutdown() {
	Window::CleanUp();
	
	if (searchBar)
		delete searchBar;
	
	for (Tab& tab : tabs)
		delete tab.editor;
	
	::deviceContext->Release();
	::deviceContext = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static bool LoadFileInformation(std::string_view path, /*out*/ BY_HANDLE_FILE_INFORMATION* fileInfo) {
	
	HANDLE hFile = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%'. LastError: %", path, FLastErr(GetLastError()));
		return false;
	}
	
	DEFER(CloseHandle(hFile));
	
	if (!GetFileInformationByHandle(hFile, fileInfo)) {
		LogError("GetFileInformationByHandle() failed. LastError: %", FLastErr(GetLastError()));
		return false;
	}
	
	return true;
}

static bool IsEditorAlreadyOpen(const MainWindow* self, std::string_view path, /*out*/ u64* tabIndex, /*out*/ u64* panelIndex) {
	
	bool hasFileInfo = false;
	
	
	// optimistic approach: just do a string compare first
	for (u64 i = 0u; i < self->tabs.size(); i++) {
		const MainWindow::Tab& tab = self->tabs[i];
		
		if (tab.editor->path == path) {
			*tabIndex = i;
			*panelIndex = tab.panelIndex;
			return true;
		}
	}
	
	BY_HANDLE_FILE_INFORMATION toFindFileInfo {};
	const bool ok = LoadFileInformation(path, &toFindFileInfo);
	if (!ok) return false;
	
	for (u64 i = 0u; i < self->tabs.size(); i++) {
		const MainWindow::Tab& tab = self->tabs[i];
		if (tab.editor->path.empty()) continue;
		
		BY_HANDLE_FILE_INFORMATION currentFileInfo {};
		const bool ok = LoadFileInformation(tab.editor->path, &currentFileInfo);
		if (!ok) continue;
		
		const bool same = toFindFileInfo.dwVolumeSerialNumber == currentFileInfo.dwVolumeSerialNumber
					   && toFindFileInfo.nFileIndexLow == currentFileInfo.nFileIndexLow
		               && toFindFileInfo.nFileIndexHigh == currentFileInfo.nFileIndexHigh;
		               
		if (same) {
			*tabIndex = i;
			*panelIndex = tab.panelIndex;
			return true;
		}
	}
	
	return false;
}

static void LogFileResult(Editor::FileResult closeResult) {
	if (closeResult == Editor::FileResult_Canceled)
		LogInfo("closing was canceled by user");
	else if (closeResult == Editor::FileResult_Failure)
		LogError("failed to save file");
}

static void RelinkPanelsAndTabs(MainWindow* self) {
	for (u64 itab = 0u; itab < self->tabs.size(); itab++) {
		MainWindow::Tab& tab = self->tabs[itab];
		if (!tab.editor) continue;
		
		for (u64 ipanel = 0; ipanel < self->panels.size(); ipanel++) {
			MainWindow::Panel& panel = self->panels[ipanel];
		
			if (tab.editor == panel.editor) {
				tab.panelIndex = ipanel;
				panel.tabIndex = itab;
				goto next_tab;
			}
		}
		
		tab.panelIndex = U64_MAX;
		next_tab: continue;
	}
}

static float GetTabHeight() {
	return settings.fontUi.lineHeight + PADDING_X2;
}

static float GetStatusBarHeight() {
	return settings.fontUi.lineHeight + PADDING_X2;
}

static void ChangeTabOfFocusedPanel(MainWindow* self, u64 tabIndexToDisplay) {
	MainWindow::Panel& panel  = self->panels[self->focusedPanelIndex];
	MainWindow::Tab&   oldTab = self->tabs[panel.tabIndex];
	MainWindow::Tab&   newTab = self->tabs[tabIndexToDisplay];
			
	oldTab.panelIndex = U64_MAX;
	newTab.panelIndex = self->focusedPanelIndex;
	panel.tabIndex    = tabIndexToDisplay;
	panel.editor      = newTab.editor;
	
	panel.editor->OnResize(D2D_RECT_F {
		.left   = (self->width / self->panels.size()) * self->focusedPanelIndex,
		.top    = GetTabHeight(),
		.right  = (self->width / self->panels.size()) * (self->focusedPanelIndex + 1),
		.bottom = self->height - GetStatusBarHeight()});
}

static void ResizePanels(MainWindow* self) {
	
	const f32 tabHeight = GetTabHeight();
	const f32 statusBarHeight = GetStatusBarHeight();
	const f32 panelWidth = std::floor(mainWindow.width / self->panels.size());
	f32 panelLeft = 0.0f;
	
	for (usize i = 0u; i < self->panels.size(); i++) {
		const MainWindow::Panel& panel = self->panels[i];
			
		const auto panelRect = D2D1_RECT_F {
			.left   = panelLeft,
			.top    = tabHeight,
			.right  = panelLeft + panelWidth,
			.bottom = mainWindow.height - statusBarHeight};
		
		panel.editor->OnResize(panelRect);
		panelLeft += panelWidth;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Open Editor
//
///////////////////////////////////////////////////////////////////////////////////////////////////

Editor* MainWindow::OpenEditor(std::string path, OpenBehavior openBehavior /*= OpenBehaviour_Default*/, bool* wasAlreadyOpen /*= nullptr*/) {
	
	//
	// check if editor already open
	//	
	if (u64 tabIndex = U64_MAX, panelIndex = U64_MAX;
		IsEditorAlreadyOpen(this, path, &tabIndex, &panelIndex)) {
		
		LogInfo("file already open: '%'", path);
		
		if (wasAlreadyOpen)
		   *wasAlreadyOpen = true;
		
		// OpenBehavior_TabOnly indicates that the user doesn't want to see the file
		// immediatly anyawy so there is no need to display anything
		if (openBehavior == MainWindow::OpenBehavior_TabOnly)
			return tabs[tabIndex].editor;
		
		// file is already open
		// if the file is displayed - focus on that panel
		// otherwise change the current panel to the file		
		if (panelIndex != U64_MAX) {
			focusedPanelIndex = panelIndex;
			
		} else {
			ChangeTabOfFocusedPanel(this, tabIndex);
		}
		
		return panels[focusedPanelIndex].editor;
		
	// 
	// file not already open...
	//
	} else {
		LogInfo("opening file: '%'", path);
		
		if (wasAlreadyOpen)
		   *wasAlreadyOpen = false;
		
		if (openBehavior == MainWindow::OpenBehavior_UpdateCurrent) {
			
			MainWindow::Panel& currentPanel = panels[focusedPanelIndex];
			MainWindow::Tab& currentTab = tabs[currentPanel.tabIndex];
			
			const auto closeResult = currentTab.editor->OpenFile(std::move(path));
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return nullptr;
			}
			
			const std::string_view title = GetFilenameFromPath(currentTab.editor->path);	
			currentTab.title.Shape(title, settings.fontUi);
			return currentTab.editor;
		
		} else {
		
			auto editor = std::make_unique<Editor>();
			if (!editor->Init())
				return nullptr;
				
			if (editor->OpenFile(std::move(path)) != Editor::FileResult_Success)
				return nullptr;
			
			MainWindow::Tab& newTab = tabs.emplace_back();
			
			const std::string_view title = GetFilenameFromPath(editor->path);	
			newTab.title.Shape(title, settings.fontUi);
			newTab.editor = editor.release();
			
			if (openBehavior == MainWindow::OpenBehavior_Default) {
				ChangeTabOfFocusedPanel(this, tabs.size() - 1);
				
			} else if (openBehavior == MainWindow::OpenBehavior_NewPanelLeft || openBehavior == MainWindow::OpenBehavior_NewPanelRight) {
				
				const u64 newPanelIndex = openBehavior == MainWindow::OpenBehavior_NewPanelLeft
					? focusedPanelIndex
					: focusedPanelIndex + 1;
						
				auto itNewPanel = panels.emplace(panels.begin() + newPanelIndex);
						
				itNewPanel->tabIndex = tabs.size() - 1;
				itNewPanel->editor = newTab.editor;
				focusedPanelIndex = newPanelIndex;
								
				RelinkPanelsAndTabs(this);
				ResizePanels(this);
			
			} else if (openBehavior == MainWindow::OpenBehavior_TabOnly) {
				newTab.panelIndex = U64_MAX;
			
			} else {
				ASSERT_UNREACHABLE;
			}
			
			return newTab.editor;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Update
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void OnActivateTab(void* ud, u64 i) {
	auto self = static_cast<MainWindow*>(ud);
	MainWindow::Tab& hitTab = self->tabs[i];
			
	// if there is already a panel for the hit tab then this panel gets focused
	// otherwise the currently focused panel becomes the hit tab item
		
	if (hitTab.panelIndex != U64_MAX)
		self->focusedPanelIndex = hitTab.panelIndex;	
	else
		ChangeTabOfFocusedPanel(self, i);
}

static void OnCloseTab(void* ud, u64 i) {
	auto self = static_cast<MainWindow*>(ud);
	
	MainWindow::Tab& tabToClose = self->tabs[i];
	
	//
	// close editor
	//
	const auto FileResult = tabToClose.editor->CloseFile();
	if (FileResult != Editor::FileResult_Success) {
		LogFileResult(FileResult);
		return;
	}
	delete tabToClose.editor;
	
	//
	// cleanup panel
	//
	if (tabToClose.panelIndex != U64_MAX) {
	
		// update panel indicies
		for (MainWindow::Tab& tab : self->tabs) {
			if (tab.panelIndex == U64_MAX) continue;
			if (tab.panelIndex > tabToClose.panelIndex) tab.panelIndex -= 1;
		}
		
		self->panels.erase(self->panels.begin() + tabToClose.panelIndex);
		if (self->focusedPanelIndex == tabToClose.panelIndex)
			self->focusedPanelIndex = self->panels.size() - 1; // we can improve this behavior but it's fine for now
			
		ResizePanels(self);
	}
	
	//
	// cleanup tab
	//
	self->tabs.erase(self->tabs.begin() + i);
	
	// update tab indicies
	for (MainWindow::Panel& panel : self->panels) {
		if (panel.tabIndex > i)
			panel.tabIndex -= 1;
	}
	
}

void MainWindow::OnUpdate() {
		
	deviceContext->BeginDraw();
	deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));
	
	const f32 tabHeight = GetTabHeight();
	
	//
	// draw panels
	//
	for (const Panel& panel : panels) {
		panel.editor->OnUpdate();
	}
	
	//
	// draw panel border
	//
	if (panels.size() >= 2) {
		for (u64 i = 0u; i < panels.size(); i++) {
			const Panel& panel = panels[i];
			
			// windows eats a few pixels of the client area when maximized
			// so that you can't see the border very well so we just it a little bit
			const D2D1_RECT_F borderRect {
				.left = panel.editor->area.left + 1.0f,
				.top = panel.editor->area.top,
				.right = panel.editor->area.right - 1.0f,
				.bottom = panel.editor->area.bottom};
			
			
			if (i == focusedPanelIndex) {
				deviceContext->DrawRectangle(borderRect, settings.GetBrushDropShadow(), 2.0f);
			} else if (RectContains(panel.editor->area, mouse.x, mouse.y)) {
				deviceContext->DrawRectangle(borderRect, settings.GetBrushHover(), 2.0f);
				if (mouse.event == Mouse::Event_Up) focusedPanelIndex = i;
			}
		}
	}
	
	//
	// draw tabs
	//
	{	
		f32 offsetX = .0f;
		u64 closedTab = U64_MAX;
		for (u64 i = 0u; i < tabs.size(); i++) {
			const Tab& tab = tabs[i];
			
			const f32 tabWidth = tab.title.width + settings.fontUi.lineHeight + PADDING_X4;
			const D2D1_RECT_F tabRect {
				.left   = offsetX,
				.top    = 0.0f,
				.right  = offsetX + tabWidth,
				.bottom = tabHeight};
						
			if (tab.panelIndex == focusedPanelIndex)
				deviceContext->FillRectangle(tabRect, settings.GetBrushDropShadow());
			
			else if (tab.panelIndex != U64_MAX)
				deviceContext->FillRectangle(tabRect, settings.GetBrushUiBackground(false));
			
			tab.title.Draw(deviceContext, PADDING + offsetX, PADDING, settings.fontUi, settings.GetBrushUiText());
			
			bool isTitleHovered = false;
			
			// check click on title
			{
				const D2D_RECT_F areaTitle {
					.left = offsetX,
					.top = 0.0f,
					.right = offsetX + tab.title.width + PADDING_X2,
					.bottom = tabHeight};
				
				if (mouse.Hittest(areaTitle, this, OnActivateTab, i)) {
					deviceContext->FillRectangle(areaTitle, settings.GetBrushHover(mouse.isDown));
					isTitleHovered = true;
				}
			}
						
			// check click on close icon
			{
				const D2D1_RECT_F areaIcon {
					.left   = PADDING + offsetX + tab.title.width + PADDING,
					.top    = 0.0f,
					.right  = PADDING + offsetX + tab.title.width + settings.fontUi.lineHeight + PADDING_X3,
					.bottom = PADDING_X2 + settings.fontUi.lineHeight};
			
				const bool isHovered = mouse.Hittest(areaIcon, this, OnCloseTab, i);
				
				ID2D1Bitmap* icon = nullptr;
				if      (tab.editor->modified && isHovered) icon = settings.icons.tabsModifiedHovered;
				else if (tab.editor->modified)              icon = settings.icons.tabsModified;
				else if (isHovered || isTitleHovered)       icon = settings.icons.tabsHovered;
				
				if (icon) {
					const D2D1_RECT_F iconTargetRect {
						.left   = areaIcon.left   + PADDING,
						.top    = areaIcon.top    + PADDING,
						.right  = areaIcon.right  - PADDING,
						.bottom = areaIcon.bottom - PADDING};
					deviceContext->FillOpacityMask(icon, settings.GetBrushUiText(), &iconTargetRect, nullptr);
					
					if (isHovered)
						deviceContext->FillRectangle(areaIcon, settings.GetBrushHover(mouse.isDown));
				}
			}
	
			offsetX += tabWidth;
		}
	}
	
	//
	// draw status bar
	//
	statusBar.OnUpdate();
	
	//
	// draw console
	//
	if (console.isOpen)
		console.OnUpdate();
	
	//
	// draw explorer
	//
	if (explorer)
		explorer->OnUpdate();
	
	//
	// draw search bar
	//
	if (searchBar) {
		searchBar->OnUpdate();
		
		if (searchBar->shouldClose) {
			delete searchBar;
			searchBar = nullptr;
		}
	}
	
	const HRESULT hr = deviceContext->EndDraw();
	
	// @TODO needs cleanup
	{
		// const double elapsed = GetElapsedMs(ticksBefore);
		// HDC hdc = GetDC(hWnd);
		// std::string str = std::format("{:.4}ms", elapsed);
		// TextOut(hdc, width - 100, 5, str.data(), str.size());
		// ReleaseDC(hWnd, hdc);
	}
	
	ValidateRect(hWnd, NULL);

	if (hr != S_OK)
		LogWarning("rendering failed. HRESULT: %", FHr(hr));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void MainWindow::OnResize(f32 newWidth, f32 newHeight) {
	ResizePanels(this);
	
	console.OnResize(newWidth, newHeight);
	
	//if (explorer)
	//	explorer->OnResize();
	
	if (searchBar)
		searchBar->OnResize();
}

bool MainWindow::OnMouseWheel(f32 distance) {
	
	if (console.isOpen && RectContains(console.area, mouse.x, mouse.y)) {
		console.OnMouseWheel(distance);
		return true;
	}
	
	if (searchBar && RectContains(searchBar->area, mouse.x, mouse.y)) {
		searchBar->OnMouseWheel(distance);
		return true;
	}
	
	
	for (const Panel& panel : panels) {
		if (RectContains(panel.editor->area, mouse.x, mouse.y)) {
			panel.editor->OnMouseWheel(distance);
			return true;
		}
	}
		
	return false;	
}

static void ActionChangeFocusedTab(MainWindow* self, bool next) {
	MainWindow::Panel& panel = self->panels[self->focusedPanelIndex];
	const u64 startIndex = panel.tabIndex;
	
	u64 i = next
		? IncrementWrapAround(startIndex, self->tabs.size())
		: DecrementWrapAround(startIndex, self->tabs.size());
	
	while (i != startIndex) {
		MainWindow::Tab& tab = self->tabs[i];
		if (tab.panelIndex == U64_MAX) {
			ChangeTabOfFocusedPanel(self, i);
			break;
		}
		
		i = next
			? IncrementWrapAround(i, self->tabs.size())
			: DecrementWrapAround(i, self->tabs.size());
	}	
}

static void ActionAddPanel(MainWindow* self, bool beforeCurrent) {
	
	MainWindow::Panel newPanel {};
	
	// find tab to display	
	for (u64 i = 0; i < self->tabs.size(); i++) {
		MainWindow::Tab& tab = self->tabs[i];
		
		if (tab.panelIndex == U64_MAX) {
			newPanel.editor = tab.editor;
			newPanel.tabIndex = i;
			goto found_tab;
		}
	}
	
	// no tab to display are left
	return;
	
found_tab:
	u64 insertIndex;
	if (beforeCurrent) {
		insertIndex = self->focusedPanelIndex;
		self->focusedPanelIndex++;
	} else {
		insertIndex =self->focusedPanelIndex;
	}
	
	self->panels.insert(self->panels.begin() + insertIndex, newPanel);
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

static void ActionSwapPanels(MainWindow* self) {

	const u64 panelIndexToSwapWith = IncrementWrapAround(self->focusedPanelIndex, self->panels.size());
	
	const u64 focusedTabIndex = self->panels[self->focusedPanelIndex].tabIndex;
	const u64 tabIndexToSwapWith = self->panels[panelIndexToSwapWith].tabIndex;
	ASSERT(focusedTabIndex != U64_MAX && tabIndexToSwapWith != U64_MAX)
		
	std::swap(
		self->panels[self->focusedPanelIndex],
		self->panels[panelIndexToSwapWith]);
		
	self->tabs[focusedTabIndex].panelIndex = panelIndexToSwapWith;
	self->tabs[tabIndexToSwapWith].panelIndex = self->focusedPanelIndex;
	self->focusedPanelIndex = panelIndexToSwapWith;
	
	ResizePanels(self);
}

static void ActionClosePanel(MainWindow* self) {
	if (self->panels.size() <= 1u) return;
	
	self->panels.erase(self->panels.begin() + self->focusedPanelIndex);
	ASSERT(!self->panels.empty());
	
	if (self->focusedPanelIndex == self->panels.size())
		self->focusedPanelIndex--;
	
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

static void ActionCloseTab(MainWindow* self) {
	if (self->tabs.size() <= 1u) return;
	
	MainWindow::Panel& panel = self->panels[self->focusedPanelIndex];
	
	// close old tab
	{
		MainWindow::Tab& oldTab = self->tabs[panel.tabIndex];
		
		const auto closeResult = oldTab.editor->CloseFile();
		if (closeResult != Editor::FileResult_Success) {
			LogFileResult(closeResult);
			return;
		}
		
		delete oldTab.editor;
		oldTab.editor = nullptr;
		
		self->tabs.erase(self->tabs.begin() + panel.tabIndex);
	}
	
	// find a new tab to display
	{
		for (u64 i = 0u; i < self->tabs.size(); i++) {
			MainWindow::Tab& tab = self->tabs[i];
			if (tab.panelIndex == U64_MAX) {
				panel.editor = tab.editor;
				goto found_new_tab;
			}
		}
	}
	
	// no more tabs to display - close the panel as well
	{
		self->panels.erase(self->panels.begin() + self->focusedPanelIndex);
		ASSERT(!self->panels.empty());
		
		if (self->focusedPanelIndex == self->panels.size())
			self->focusedPanelIndex--;
	}
	
found_new_tab:
	RelinkPanelsAndTabs(self);
	ResizePanels(self);	
}

static void ActionCloseTabAndPanel(MainWindow* self) {
	if (self->tabs.size() <= 1u) return;
	
	MainWindow::Panel& panel = self->panels[self->focusedPanelIndex];
	
	// close old tab
	{
		MainWindow::Tab& oldTab = self->tabs[panel.tabIndex];
		
		const auto closeResult = oldTab.editor->CloseFile();
		if (closeResult != Editor::FileResult_Success) {
			LogFileResult(closeResult);
			return;
		}
		
		delete oldTab.editor;
		oldTab.editor = nullptr;
		
		self->tabs.erase(self->tabs.begin() + panel.tabIndex);
	}
	
	// close panel
	{
		self->panels.erase(self->panels.begin() + self->focusedPanelIndex);
		ASSERT(!self->panels.empty());
		
		if (self->focusedPanelIndex == self->panels.size())
			self->focusedPanelIndex--;
	}
	
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

void MainWindow::OnKeyDown(KeyEvent event) {
		
	if (event == settings.keybinds.showFileSearch) {
		if (searchBar) return;
		
		searchBar = FileSearchBar::Make();
			
	} else if (event == settings.keybinds.showExplorer) {
		if (explorer) {
			delete explorer;
			explorer = nullptr;
		} else {
			explorer = Explorer::Make();
		}

	} else if (event == settings.keybinds.showToolSearch) {
		if (searchBar) return;
		searchBar = ToolSearchBar::Make();
		
	} else if (event == settings.keybinds.showConsole) {
		console.isOpen = !console.isOpen;
	
	} else if (event == settings.keybinds.focusNextTab) {
		ActionChangeFocusedTab(this, true);

	} else if (event == settings.keybinds.focusPrevTab) {
		ActionChangeFocusedTab(this, false);
	
	} else if (event == settings.keybinds.focusNextPanel) {
		focusedPanelIndex = IncrementWrapAround(focusedPanelIndex, panels.size());
	
	} else if (event == settings.keybinds.focusPrevPanel) {
		focusedPanelIndex = DecrementWrapAround(focusedPanelIndex, panels.size());
		
	} else if (event == settings.keybinds.addPanelAfter) {
		ActionAddPanel(this, false);
	
	} else if (event == settings.keybinds.addPanelBefore) {
		ActionAddPanel(this, true);
		
	} else if (event == settings.keybinds.swapPanels) {
		ActionSwapPanels(this);
	
	} else if (event == settings.keybinds.closePanel) {
		ActionClosePanel(this);
		
	} else if (event == settings.keybinds.closeTab) {
		ActionCloseTab(this);
	
	} else if (event == settings.keybinds.closePanelAndTab) {
		ActionCloseTabAndPanel(this);

	} else if (console.isOpen) {
		console.OnKeyDown(event);
	
	} else if (explorer) {
		bool shouldClose = false;
		explorer->OnKeyDown(event);
		
		if (explorer->shouldClose) {
			delete explorer;
			explorer = nullptr;
		}
		
	} else if (searchBar) {
		searchBar->OnKeyDown(event);
		
		if (searchBar->shouldClose) {
			delete searchBar;
			searchBar = nullptr;
		}
	
	} else {
		ASSERT(focusedPanelIndex < panels.size())
		panels[focusedPanelIndex].editor->OnKeyDown(event);
	}
}

void MainWindow::OnChar(const char* data, u64 len) {
		
	if (searchBar) {
		searchBar->OnChar(data, len);

	} else if (explorer) {
		explorer->OnChar(data, len);
		
	} else {
		ASSERT(focusedPanelIndex < panels.size())
		panels[focusedPanelIndex].editor->OnChar(data, len);
	}
}

bool MainWindow::OnClose() {	
	bool shouldClose = true;
	for (const Tab& tab : tabs)
		shouldClose = shouldClose && tab.editor->OnClose();
		
	return shouldClose;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

MainWindow::OpenBehavior OpenBehaviorFromModifiers(KeyEvent event) {
	if      (event.ctrl)                return MainWindow::OpenBehavior_UpdateCurrent;
	else if (event.shift && !event.alt) return MainWindow::OpenBehavior_NewPanelRight;
	else if (event.shift &&  event.alt) return MainWindow::OpenBehavior_NewPanelLeft;
	return MainWindow::OpenBehavior::OpenBehavior_Default;
}
