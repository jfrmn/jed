#include "main-window.hh"
#include "basic.hh"
#include "key-bindings.hh"
#include "globals.hh"

#include "file-search-bar.hh"
#include "explorer.hh"
#include "console.hh"
#include "commands/tool-search-bar.hh"

#include "editor/editor.hh"
#include "ui/constants.h"
#include "ui/style.hh"

#include "util/logging.hh"
#include "util/file-util.hh"
#include "util/rect-util.hh"

#include "graphics/effects.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
// #include <d2d1_1.h>
#include <d2d1_3.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Helper
//

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
	return style.fontUi.lineHeight + PADDING_X2;
}

static float GetStatusBarHeight() {
	return style.fontUi.lineHeight + PADDING_X2;
}

static void ChangeTabOfFocusedPanel(MainWindow* self, u64 tabIndexToDisplay) {
	MainWindow::Panel& panel = self->panels[self->focusedPanelIndex];
	MainWindow::Tab&   oldTab  = self->tabs[panel.tabIndex];
	MainWindow::Tab&   newTab  = self->tabs[tabIndexToDisplay];
			
	oldTab.panelIndex  = U64_MAX;
	newTab.panelIndex  = self->focusedPanelIndex;
	panel.tabIndex     = tabIndexToDisplay;
	panel.editor       = newTab.editor;
	
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
		
		// @FIXME
		// this might become a dangling pointer!!
		// we move the path into the editor which which by itself is not a problem
		// except when it's a short string and SSO kicks in...
		const std::string_view title = GetFilenameFromPath(path);

		if (openBehavior == MainWindow::OpenBehavior_UpdateCurrent) {
			
			MainWindow::Panel& currentPanel = panels[focusedPanelIndex];
			MainWindow::Tab& currentTab = tabs[currentPanel.tabIndex];
			
			const auto closeResult = currentTab.editor->OpenFile(std::move(path));
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return nullptr;
			}
			
			currentTab.title = title;
			currentTab.tabWidth = style.fontUi.MeasureText(title) + style.fontUi.lineHeight + PADDING_X3;
			return currentTab.editor;
		
		} else {
		
			auto editor = std::make_unique<Editor>();
			if (!editor->Init())
				return nullptr;
				
			if (editor->OpenFile(std::move(path)) != Editor::FileResult_Success)
				return nullptr;
			
			MainWindow::Tab& newTab = tabs.emplace_back();
			newTab.title = title,
			newTab.tabWidth = style.fontUi.MeasureText(title) + style.fontUi.lineHeight + PADDING_X3,
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Update
//

static void OnClickTab(void* ud, u64 i) {}

void MainWindow::OnUpdate() {
		
	deviceContext->BeginDraw();
	deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));
	
	//
	// draw panels
	//
	{
		const float tabHeight = GetTabHeight();
		
		for (usize i = 0u; i < panels.size(); i++) {
			const Panel& panel = panels[i];
			
			panel.editor->OnUpdate();
		
			const bool drawFocusedPanelBorder = (panels.size() >= 2) && (i == focusedPanelIndex);
			const bool drawHoverPanelBorder   = (hoveredTabIndex != U64_MAX) && (panel.tabIndex == hoveredTabIndex);
					
			if (drawFocusedPanelBorder || drawHoverPanelBorder) {
										
				// windows eats a few pixels of the client area when maximized
				// you can't see the border very well then so we just offset it a little
				D2D1_RECT_F borderRect = panel.editor->area;
				borderRect.left += 1.0f;
				borderRect.right -= 1.0f;
						
				if (drawFocusedPanelBorder)
					deviceContext->DrawRectangle(borderRect, style.GetBrushSelection(), 2.0f);
							
				if (drawHoverPanelBorder)
					deviceContext->DrawRectangle(borderRect, style.GetBrushHover(), 2.0f);
			}
		}
	}
	
	//
	// draw tabs
	//
	{	
		const float tabHeight = GetTabHeight();
	
		GlyphRunShapingMemory mem;
		GlyphRun run;
		
		f32 offsetX = .0f;
		u64 closedTab = U64_MAX;
		for (u64 i = 0u; i < tabs.size(); i++) {
			const Tab& tab = tabs[i];
			
			const D2D1_RECT_F tabRect {
				.left   = offsetX,
				.top    = 0.0f,
				.right  = offsetX + tab.tabWidth,
				.bottom = tabHeight };
						
			if (tab.panelIndex == focusedPanelIndex) {
				deviceContext->FillRectangle(tabRect, style.GetBrushGlow());
			
			} else if (tab.panelIndex != U64_MAX) {
				deviceContext->FillRectangle(tabRect, style.GetBrushUiBackground(false));
			}
			
			run.Shape(tab.title, style.fontUi, &mem);
			run.Draw(deviceContext,
				D2D1_POINT_2F {
					.x = PADDING + offsetX,
					.y = PADDING },
				style.fontUi,
				style.GetBrushUiText());
						
			const bool isHovered = mouse.Hittest(tabRect, this, OnClickTab, i);
			
			bool isCloseIconHovered = false;
			
			// draw tab icon
			{
				ID2D1Bitmap* icon = nullptr;
				if      (tab.editor->modified && isHovered) icon = style.icons[Style::Icon_Tabs_ModifiedHovered];
				else if (tab.editor->modified)              icon = style.icons[Style::Icon_Tabs_Modified];
				else if (isHovered)                         icon = style.icons[Style::Icon_Tabs_Hovered];
					
				if (icon) {
					const f32 totalGlyphAdvances = run.GetTotalAdvance();
					const D2D1_RECT_F iconHitbox {
						.left   = PADDING + offsetX + totalGlyphAdvances,
						.top    = 0.0f,
						.right  = PADDING + offsetX + totalGlyphAdvances + style.fontUi.lineHeight + PADDING_X2,
						.bottom = PADDING_X2 + style.fontUi.lineHeight};
					
					const D2D1_RECT_F iconTargetRect {
						.left   = iconHitbox.left + PADDING,
						.top    = iconHitbox.top + PADDING,
						.right  = iconHitbox.right - PADDING,
						.bottom = iconHitbox.bottom - PADDING};
					deviceContext->FillOpacityMask(icon, style.GetBrushUiText(), &iconTargetRect, nullptr);
					
					if (RectContains(iconHitbox, mouse.x, mouse.y)) {
						deviceContext->FillRectangle(iconHitbox, style.GetBrushHover(mouse.isDown));
						isCloseIconHovered = true;
					}
				}
			}
			
			if (isHovered && !isCloseIconHovered)
				deviceContext->FillRectangle(tabRect, style.GetBrushHover(mouse.isDown));
			
			offsetX += tab.tabWidth;
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


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Resize
//

void MainWindow::OnResize(f32 newWidth, f32 newHeight) {
	ResizePanels(this);
	
	console.OnResize(newWidth, newHeight);
	
	//if (explorer)
	//	explorer->OnResize();
	
	if (searchBar)
		searchBar->OnResize();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Input
//

static void Hittest(const MainWindow* self, float mx, float my, u64* hitPanel, u64* hitTab, bool* hitCloseButton) {
	
	const float tabHeight = GetTabHeight();
	
	// y is on tab height
	if (my > 0.0f && my < tabHeight) {
		
		float offsetX = .0f;
		for (u64 i = 0; i < self->tabs.size(); i++) {
			const MainWindow::Tab& tab = self->tabs[i];
			
			offsetX += tab.tabWidth;
			if (mx < offsetX) {
				*hitCloseButton = (mx >= (offsetX - style.fontUi.lineHeight - PADDING_X2));
				*hitTab = i;
				*hitPanel = U64_MAX;
				return;
			}
		}
	
	} else if (my > tabHeight && my < self->height) {
				
		// @TODO respect status bar
		for (u64 i = 0; i < self->panels.size(); i++) {
			const MainWindow::Panel& panel = self->panels[i];
		
			if (mx > panel.editor->area.left &&
				mx < panel.editor->area.right) {
				
				*hitCloseButton = false;
				*hitTab = U64_MAX;
				*hitPanel = i;
				return;
			}
		}
	}
	
	*hitCloseButton = false;
	*hitTab = U64_MAX;
	*hitPanel = U64_MAX;
}

bool MainWindow::OnMouseDown(MouseEvent event) {
	u64 hitPanelIndex, hitTabIndex; bool hitCloseButton;
	Hittest(this, event.x, event.y, &hitPanelIndex, &hitTabIndex, &hitCloseButton);
	
	if (hitPanelIndex != U64_MAX) {
		focusedPanelIndex = hitPanelIndex;
	 	//panels[hitPanelIndex].editor->OnMouseDown(event);
		return true;
	}
	
	return false;
}

bool MainWindow::OnMouseUp(MouseEvent event) {

	u64 hitPanelIndex, hitTabIndex; bool hitCloseButton;
	Hittest(this, event.x, event.y, &hitPanelIndex, &hitTabIndex, &hitCloseButton);
	
	// hit a tab but not its close button
	if (hitTabIndex != U64_MAX && !hitCloseButton) {
		Tab& hitTab = tabs[hitTabIndex];
			
		// if there is already a panel for the hit tab then this panel gets focused
		// otherwise the currently focused panel becomes the hit tab item
			
		if (hitTab.panelIndex != U64_MAX)
			focusedPanelIndex = hitTab.panelIndex;	
		else
			ChangeTabOfFocusedPanel(this, hitTabIndex);
		
		return true;
	
	// hit a close button of a tab
	} else if (hitTabIndex != U64_MAX && hitCloseButton) {
		auto itHitTab = tabs.begin() + hitTabIndex;
		
		//
		// close editor
		//
		const auto FileResult = itHitTab->editor->CloseFile();
		if (FileResult != Editor::FileResult_Success) {
			LogFileResult(FileResult);
			return true;
		}
		
		delete itHitTab->editor;
		
		//
		// cleanup panel
		//
		if (itHitTab->panelIndex != U64_MAX && panels.size() >= 2) {
		
			// update panel indicies
			for (Tab& tab : tabs) {
				if (tab.panelIndex == U64_MAX) continue;
				if (tab.panelIndex  > itHitTab->panelIndex) tab.panelIndex -= 1;
			}
			
			panels.erase(panels.begin() + itHitTab->panelIndex);
			if (focusedPanelIndex == itHitTab->panelIndex)
				focusedPanelIndex  = panels.size() - 1; // we can improve this behavior but it's fine for now
			
			// update panel indicies
			for (Tab& tab : tabs) {
				if (tab.panelIndex == U64_MAX) continue;
				if (tab.panelIndex  > itHitTab->panelIndex) tab.panelIndex -= 1;
			}
		}
		
		//
		// cleanup tab
		//
		tabs.erase(itHitTab);
		
		// update tab indicies
		for (Panel& panel : panels) {
			if (panel.tabIndex > hitTabIndex)
				panel.tabIndex -= 1;
		}
		
		return true;
	
	// hit a panel
	} else if (hitPanelIndex != U64_MAX) {
		focusedPanelIndex = hitPanelIndex;
		return true;
	
	} else {
		return false;
	}
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
	
	
	u64 hitPanelIndex, hitTabIndex; bool hitCloseButton;
	Hittest(this, mouse.x, mouse.y, &hitPanelIndex, &hitTabIndex, &hitCloseButton);
	
	if (hitPanelIndex != U64_MAX) {
		Panel& panel = panels[hitPanelIndex];
		panel.editor->OnMouseWheel(distance);
		return true;
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

	const u64 indexToSwapWith = IncrementWrapAround(self->focusedPanelIndex, self->panels.size());
		
	std::swap(
		self->panels[self->focusedPanelIndex],
		self->panels[indexToSwapWith]);
		
	self->tabs[self->focusedPanelIndex].panelIndex = indexToSwapWith;
	self->tabs[indexToSwapWith].panelIndex = self->focusedPanelIndex;
		
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
		
	if (event == keybinds.actions.showFileSearch) {
		if (searchBar) return;
		
		searchBar = FileSearchBar::Make();
			
	} else if (event == keybinds.actions.showExplorer) {
		if (explorer) {
			delete explorer;
			explorer = nullptr;
		} else {
			explorer = Explorer::Make();
		}

	} else if (event == keybinds.actions.showToolSearch) {
		if (searchBar) return;
		searchBar = ToolSearchBar::Make();
		
	} else if (event == keybinds.actions.showConsole) {
		console.isOpen = !console.isOpen;
	
	} else if (event == keybinds.actions.focusNextTab) {
		ActionChangeFocusedTab(this, true);

	} else if (event == keybinds.actions.focusPrevTab) {
		ActionChangeFocusedTab(this, false);
	
	} else if (event == keybinds.actions.focusNextPanel) {
		focusedPanelIndex = IncrementWrapAround(focusedPanelIndex, panels.size());
	
	} else if (event == keybinds.actions.focusPrevPanel) {
		focusedPanelIndex = DecrementWrapAround(focusedPanelIndex, panels.size());
		
	} else if (event == keybinds.actions.addPanelAfter) {
		ActionAddPanel(this, false);
	
	} else if (event == keybinds.actions.addPanelBefore) {
		ActionAddPanel(this, true);
		
	} else if (event == keybinds.actions.swapPanels) {
		ActionSwapPanels(this);
	
	} else if (event == keybinds.actions.closePanel) {
		ActionClosePanel(this);
		
	} else if (event == keybinds.actions.closeTab) {
		ActionCloseTab(this);
	
	} else if (event == keybinds.actions.closePanelAndTab) {
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
