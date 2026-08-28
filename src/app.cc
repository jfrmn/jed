#include "app.hh"
#include "basic.hh"
#include "globals.hh"
#include "settings.hh"
#include "file-watcher.hh"
#include "explorer.hh"
#include "tool-output.hh"
#include "logging.hh"
#include "util.hh"

#include "graphics.hh"
#include "editor/editor.hh"
#include "ui/constants.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d2d1_3.h>
#include <shlobj.h>

App app {};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Creation and Shutdown
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool App::Init() {
	ASSERT(mainWindow.deviceContext);
	
	::deviceContext = mainWindow.deviceContext;

	if (!statusBar.Init()) {
		LogError("init status bar failed");
		return false;
	}
	
	if (!toolOutput.Init()) {
		LogError("init status bar failed");
		return false;
	}

	searchBarFiles.Init();
	searchBarTools.Init();
	searchBarCommands.Init();
	return true;
}

void App::Shutdown() {
	::deviceContext = nullptr;
	for (Tab& tab : tabs)
		delete tab.editor;	
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helper
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static bool LoadFileInformation(std::string_view path, /*out*/ BY_HANDLE_FILE_INFORMATION* fileInfo) {
	
	HANDLE hFile = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%.*s'. LastError: %s", SIZE_AND_DATA(path), StrLastErr(GetLastError()));
		return false;
	}
	
	DEFER(CloseHandle(hFile));

	if (!GetFileInformationByHandle(hFile, fileInfo)) {
		LogError("GetFileInformationByHandle() failed. LastError: %s", StrLastErr(GetLastError()));
		return false;
	}
		
	return true;
}

static bool FindEditor(const App* self, std::string_view path, /*out*/ u64* tabIndex, /*out*/ u64* panelIndex) {
		
	// optimistic approach: just do a string compare first
	for (u64 i = 0u; i < self->tabs.size(); i++) {
		const App::Tab& tab = self->tabs[i];
		
		if (tab.editor->path == path) {
			*tabIndex = i;
			*panelIndex = tab.panelIndex;
			return true;
		}
	}
	
	BY_HANDLE_FILE_INFORMATION toFindFileInfo {};
	if (!LoadFileInformation(path, &toFindFileInfo)) return false;
	
	for (u64 i = 0u; i < self->tabs.size(); i++) {
		const App::Tab& tab = self->tabs[i];
		if (tab.editor->path.empty()) continue;
		
		BY_HANDLE_FILE_INFORMATION fileInfo {};
		if (!LoadFileInformation(tab.editor->path, &fileInfo)) continue;
		
		const bool same = toFindFileInfo.dwVolumeSerialNumber == fileInfo.dwVolumeSerialNumber
					   && toFindFileInfo.nFileSizeHigh == fileInfo.nFileIndexHigh
					   && toFindFileInfo.nFileSizeLow == fileInfo.nFileIndexLow;
		
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

static void RelinkPanelsAndTabs(App* self) {
	for (u64 itab = 0u; itab < self->tabs.size(); itab++) {
		App::Tab& tab = self->tabs[itab];
		if (!tab.editor) continue;
		
		for (u64 ipanel = 0; ipanel < self->panels.size(); ipanel++) {
			App::Panel& panel = self->panels[ipanel];
		
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

static float TabHeight() {
	return settings.fontUi.lineHeight + PADDING_X2;
}

static float StatusBarHeight() {
	return settings.fontUi.lineHeight + PADDING_X2;
}

static void ChangeTabOfFocusedPanel(App* self, u64 tabIndexToDisplay) {
	App::Panel& panel  = self->panels[self->focusedPanelIndex];
	App::Tab&   oldTab = self->tabs[panel.tabIndex];
	App::Tab&   newTab = self->tabs[tabIndexToDisplay];
			
	oldTab.panelIndex = U64_MAX;
	newTab.panelIndex = self->focusedPanelIndex;
	panel.tabIndex    = tabIndexToDisplay;
	panel.editor      = newTab.editor;
	
	panel.editor->OnResize(D2D_RECT_F {
		.left   = (mainWindow.width / self->panels.size()) * self->focusedPanelIndex,
		.top    = TabHeight(),
		.right  = (mainWindow.width / self->panels.size()) * (self->focusedPanelIndex + 1),
		.bottom = mainWindow.height - StatusBarHeight()});
}

static void ResizePanels(App* self) {
	
	const f32 tabHeight = TabHeight();
	const f32 statusBarHeight = StatusBarHeight();
	const f32 panelWidth = std::floor(mainWindow.width / self->panels.size());
	f32 panelLeft = 0.0f;
	
	for (usize i = 0u; i < self->panels.size(); i++) {
		const App::Panel& panel = self->panels[i];
			
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

Editor* App::OpenEditor(std::string path, OpenBehavior openBehavior /*= OpenBehaviour_Default*/, bool* wasAlreadyOpen /*= nullptr*/) {
	
	//
	// check if editor already open
	//
	if (u64 tabIndex = U64_MAX, panelIndex = U64_MAX;
		FindEditor(this, path, &tabIndex, &panelIndex)) {
		
		LogInfo("file already open: '%.*s'", SIZE_AND_DATA(path));
		
		if (wasAlreadyOpen)
		   *wasAlreadyOpen = true;
		
		// OpenBehavior_TabOnly indicates that the user doesn't want to see the file
		// immediatly anyawy so there is no need to display anything
		if (openBehavior == App::OpenBehavior_TabOnly)
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
		LogInfo("opening file: '%.*s'", SIZE_AND_DATA(path));
		
		if (wasAlreadyOpen)
		   *wasAlreadyOpen = false;
		
		if (openBehavior == App::OpenBehavior_UpdateCurrent && !panels.empty()) {
			ASSERT(focusedPanelIndex != U64_MAX);
			ASSERT(!tabs.empty());
			
			App::Panel& currentPanel = panels[focusedPanelIndex];
			App::Tab& currentTab = tabs[currentPanel.tabIndex];
			
			const auto closeResult = currentTab.editor->OpenFile(std::move(path));
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return nullptr;
			}
			
			const std::string_view title = GetFilenameFromPath(currentTab.editor->path);	
			currentTab.title.Shape(title, settings.fontUi);
			
			fileWatcher.SubscribeDirectoryOfFile(currentTab.editor->path);
			
			return currentTab.editor;
		
		} else {
		
			auto editor = std::make_unique<Editor>();
			if (!editor->Init())
				return nullptr;
				
			if (editor->OpenFile(std::move(path)) != Editor::FileResult_Success)
				return nullptr;
			
			App::Tab& newTab = tabs.emplace_back();
			
			const std::string_view title = GetFilenameFromPath(editor->path);	
			newTab.title.Shape(title, settings.fontUi);
			newTab.editor = editor.release();
			
			fileWatcher.SubscribeDirectoryOfFile(newTab.editor->path);
			
			if (panels.empty()) {
				ASSERT(focusedPanelIndex == U64_MAX);
				App::Panel& panel = panels.emplace_back();
				panel.editor = newTab.editor;
				panel.tabIndex = newTab.panelIndex = focusedPanelIndex = 0u;
				ResizePanels(this);
				
			} else if (openBehavior == App::OpenBehavior_Default) {
				ChangeTabOfFocusedPanel(this, tabs.size() - 1);
				
			} else if (openBehavior == App::OpenBehavior_NewPanelLeft || openBehavior == App::OpenBehavior_NewPanelRight) {
				
				const u64 newPanelIndex = openBehavior == App::OpenBehavior_NewPanelLeft
					? focusedPanelIndex
					: focusedPanelIndex + 1;
						
				auto itNewPanel = panels.emplace(panels.begin() + newPanelIndex);
						
				itNewPanel->tabIndex = tabs.size() - 1;
				itNewPanel->editor = newTab.editor;
				focusedPanelIndex = newPanelIndex;
								
				RelinkPanelsAndTabs(this);
				ResizePanels(this);
			
			} else if (openBehavior == App::OpenBehavior_TabOnly) {
				newTab.panelIndex = U64_MAX;
			
			} else if (panels.empty()) {
				auto newPanel = panels.emplace_back();
				newPanel.tabIndex = tabs.size() - 1;
				newPanel.editor = newTab.editor;
				focusedPanelIndex = 0u;
				newTab.panelIndex = 0u;
				
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
	auto self = static_cast<App*>(ud);
	App::Tab& hitTab = self->tabs[i];
			
	// if there is already a panel for the hit tab then this panel gets focused
	// otherwise the currently focused panel becomes the hit tab item
		
	if (hitTab.panelIndex != U64_MAX)
		self->focusedPanelIndex = hitTab.panelIndex;	
	else
		ChangeTabOfFocusedPanel(self, i);
}

static void OnCloseTab(void* ud, u64 i) {
	auto self = static_cast<App*>(ud);
	
	App::Tab& tabToClose = self->tabs[i];
	
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
		for (App::Tab& tab : self->tabs) {
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
	for (App::Panel& panel : self->panels) {
		if (panel.tabIndex > i)
			panel.tabIndex -= 1;
	}
	
}

static void OnClickStartPage(void* ud, u64 btn) {
	auto self = static_cast<App*>(ud);
	if (btn == 0) { // search file
		if (self->searchBar) return;
		self->searchBar = &self->searchBarFiles;
		needsUpdate = true;
	
	} else if (btn == 1) { // open explorer
		if (self->explorer) return;
		self->explorer = Explorer::Make();
		needsUpdate = true;
	
	} else if (btn == 2) { // open file dialog
		IFileDialog* fileDialog = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,  IID_PPV_ARGS(&fileDialog));
		if (hr != S_OK) {
			LogError("CoCreateInstance() failed. HRESULT: %", StrHr(hr));
			return;
		}
		DEFER(fileDialog->Release());
		
		DWORD flags = 0;
		fileDialog->GetOptions(&flags);
		fileDialog->SetOptions(flags | FOS_FORCEFILESYSTEM);
		hr = fileDialog->Show(mainWindow.hWnd);
		if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			LogInfo("User cancelled open file dialog.");
			return;
		} else if (hr != S_OK) {
			LogError("Show() failed. HRESULT: %", StrHr(hr));
			return;
		}
		
		IShellItem* resultItem = nullptr;
		hr = fileDialog->GetResult(&resultItem);
		if (hr != S_OK) {
			LogError("GetResult() failed. HRESULT: %", StrHr(hr));
			return;
		}
		DEFER(resultItem->Release());
		
		wchar* wfilepath = nullptr;
  		hr = resultItem->GetDisplayName(SIGDN_FILESYSPATH, &wfilepath);
  		if (hr != S_OK) {
  			LogError("GetDisplayName() failed. HRESULT: %", StrHr(hr));
			return;
  		}
  		
  		std::string filepath {};
  		u64 filepathLen = 0u;
  		ToUtf8(wfilepath, {}, &filepathLen);
  		
  		filepath.resize(filepathLen);
  		ToUtf8(wfilepath, {filepath.data(), filepath.size()}, nullptr);
  		CoTaskMemFree(wfilepath);
  		
  		LogInfo("open dialog result: %.*s", SIZE_AND_DATA(filepath));
  		self->OpenEditor(std::move(filepath));
	
	} else if (btn == 3) { // open manual
		// @TODO not implemented
		// (there is no manual)
	} else {
		ASSERT_UNREACHABLE;
	}
}

static void FinishDrawing(App* self) {
	const HRESULT hr = deviceContext->EndDraw();
	if (hr == S_OK) return;
	
	const std::string errorString = FormatString("Rendering failed. HRESULT: %s", StrHr(hr));
	LogError("%s", errorString.c_str());
	
	HDC hDC = GetDC(mainWindow.hWnd);
	
	SIZE textExtend {};
	GetTextExtentPointA(hDC, errorString.data(), static_cast<int>(errorString.size()), &textExtend);
	SetBkColor(hDC, RGB(0, 0, 0));
	SetTextColor(hDC, RGB(255, 0, 0));
	TextOut(hDC,
		static_cast<int>((mainWindow.width / 2.0f) - (textExtend.cx / 2.0f)),
		static_cast<int>((mainWindow.height / 2.0f) - (textExtend.cy / 2.0f)),
		errorString.data(),
		static_cast<int>(errorString.size()));
	ReleaseDC(mainWindow.hWnd, hDC);		
	ValidateRect(mainWindow.hWnd, NULL);
}

void App::Update() {
	deviceContext->BeginDraw();
	deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));
	DEFER(FinishDrawing(this));
	
	const f32 tabHeight = TabHeight();
	
	//
	// fill background
	//
	deviceContext->FillRectangle(
		D2D_RECT_F {
			.left = 0.0f,
			.top = tabHeight,
			.right = mainWindow.width,
			.bottom = mainWindow.height - tabHeight},
		settings.GetBrushEditorBackground());
	
	//
	// draw panels
	//
	for (const Panel& panel : panels) {
		panel.editor->Update();
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
				if (mainWindow.event.type == Event::Type_MouseDown) focusedPanelIndex = i;
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
			
			if (tab.editor->fileRemoved) {
				deviceContext->DrawLine(
					D2D_POINT_2F {PADDING + offsetX,                   PADDING + settings.fontUi.strikethroughOffset},
					D2D_POINT_2F {PADDING + offsetX + tab.title.width, PADDING + settings.fontUi.strikethroughOffset},
					settings.GetBrushUiText());
			}
			
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
						
					// draw preview panel
					{
						const TextPosition caret = tab.editor->textController.carets.front().position;
						const s64 fromLine = std::max(static_cast<s64>(caret.line) - 2, 0ll);
						const s64 toLine =   std::min(static_cast<s64>(caret.line) + 2,
													  static_cast<s64>(tab.editor->textController.buffer.GetMaxLine()));
						
						staticGlyphRun.Shape(tab.editor->path, settings.fontUi);
						
						const u64 lineCount = (toLine - fromLine + 1);
						const f32 width = std::max(300.0f, staticGlyphRun.width + PADDING_X2);
						
						const D2D_RECT_F previewPanelArea {
							.left = areaTitle.left,
							.top = areaTitle.bottom,
							.right = areaTitle.left + width,
							.bottom = areaTitle.bottom + settings.fontUi.lineHeight + PADDING_X4 + (lineCount * settings.fontEditor.lineHeight)};
						
						// blur background
						BlurArea(deviceContext, previewPanelArea);
						
						deviceContext->PushAxisAlignedClip(previewPanelArea, D2D1_ANTIALIAS_MODE_ALIASED);
						
						// draw header						
						deviceContext->FillRoundedRectangle(ToRounded(
							MakeRect(previewPanelArea.left, previewPanelArea.top, width, settings.fontUi.lineHeight + PADDING_X2)),
							settings.GetBrushUiBackground());
						
						staticGlyphRun.Draw(deviceContext,
							previewPanelArea.left + PADDING,
							previewPanelArea.top + PADDING,
							settings.fontUi,
							settings.GetBrushUiText(false));
						
						auto GetYOffsetForLine = [fromLine] (u64 ln) {
							return PADDING_X3 + settings.fontUi.lineHeight + (settings.fontEditor.lineHeight * (ln-fromLine));
						};
						
						// draw lines
						for (s64 i = fromLine; i <= toLine; i++) {
							tab.editor->glyphRuns[i].Draw(deviceContext,
								previewPanelArea.left + PADDING,
								previewPanelArea.top + GetYOffsetForLine(i),
								settings.fontEditor,
								settings.GetBrushEditorText());
						}
						
						// draw caret
						{
							const f32 offsetX = PADDING + tab.editor->glyphRuns[caret.line].MeasureOffset(caret.character);
							const f32 offsetY = GetYOffsetForLine(caret.line);
							
							deviceContext->DrawRectangle(
								D2D_RECT_F {
									.left = previewPanelArea.left + offsetX,
									.top = previewPanelArea.top + offsetY,
									.right = previewPanelArea.left + offsetX + settings.fontEditor.spaceAdvance,
									.bottom = previewPanelArea.top + offsetY + settings.fontUi.lineHeight},
								settings.GetBrushEditorText());
						}
						
						deviceContext->PopAxisAlignedClip();
					}
					
					// draw grey box around panel
					if (tab.panelIndex != U64_MAX) {
						deviceContext->DrawRectangle(
							D2D1_RECT_F {
								.left = tab.editor->area.left + 1.0f,
								.top = tab.editor->area.top,
								.right = tab.editor->area.right - 1.0f,
								.bottom = tab.editor->area.bottom},
							settings.GetBrushHover(),
							2.0f);
					}
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
				if      (tab.editor->isDirty && isHovered) icon = settings.icons.tabsModifiedHovered;
				else if (tab.editor->isDirty)              icon = settings.icons.tabsModified;
				else if (isHovered || isTitleHovered)      icon = settings.icons.tabsHovered;
				
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
	// draw start page if no panels/tabs are open
	//
	if (tabs.empty()) {
		ASSERT(panels.empty());
		
		constexpr u64 BUTTON_COUNT = 4u;
		std::string_view buttonTexts[BUTTON_COUNT] {"Search for a file", "Open Explorer", "Open a file", "Open manual"};
		GlyphRun buttonRuns[BUTTON_COUNT] {};
		
		static_assert(STATIC_ARRAY_SIZE(buttonTexts) == BUTTON_COUNT);
		static_assert(STATIC_ARRAY_SIZE(buttonRuns) == BUTTON_COUNT);
		
		GlyphRun::ShapeBatch(buttonTexts, settings.fontUi, buttonRuns);
		
		const f32 buttonWidth = std::max_element(buttonRuns, buttonRuns + BUTTON_COUNT, [] (const auto& lhs, const auto& rhs) { return lhs.width < rhs.width; })->width + PADDING_X2;
		const f32 totalHeight = ((settings.fontUi.lineHeight + PADDING_X2) * BUTTON_COUNT) + (MARGIN_X2 * BUTTON_COUNT-1);
		
		f32 offsetY = 0.0f;
		for (u64 i = 0u; i < BUTTON_COUNT; i++) {
			
			const D2D_RECT_F buttonRect {
				.left   = (mainWindow.width  / 2.0f) - (buttonWidth / 2.0f),
				.top    = (mainWindow.height / 2.0f) - (totalHeight / 2.0f) + offsetY,
				.right  = (mainWindow.width  / 2.0f) - (buttonWidth / 2.0f) + buttonWidth,
				.bottom = (mainWindow.height / 2.0f) - (totalHeight / 2.0f) + offsetY + (settings.fontUi.lineHeight + PADDING_X2)};
			
			deviceContext->DrawRoundedRectangle(ToRounded(buttonRect), settings.GetBrushUiBackground());
			
			if ((i == 0 && searchBar) || i == 1 && explorer)
				deviceContext->FillRoundedRectangle(ToRounded(buttonRect), settings.GetBrushUiBackground());
			
			buttonRuns[i].DrawCenter(deviceContext, buttonRect.left, buttonRect.top + PADDING, buttonWidth, settings.fontUi, settings.GetBrushUiText());
			
			if (mouse.Hittest(buttonRect, this, OnClickStartPage, i))
				deviceContext->FillRoundedRectangle(ToRounded(buttonRect), settings.GetBrushHover(mouse.isDown));
			
			offsetY += settings.fontUi.lineHeight + PADDING_X2 + MARGIN_X2;
		}
	}
	
	//
	// draw status bar 
	//
	statusBar.OnUpdate();
	
	//
	 // draw console
	//
	if (toolOutput.isOpen)
		toolOutput.Update();
	
	//
	// draw explorer
	//
	if (explorer)
		explorer->Update();
	
	//
	// draw search bar
	//
	if (searchBar) {
		searchBar->OnUpdate();
		
		if (searchBar->shouldClose)
			searchBar = nullptr;
	}
}

Editor* App::GetFocusedEditor() {
	if (focusedPanelIndex != U64_MAX) {
		ASSERT(focusedPanelIndex < panels.size());
		return panels[focusedPanelIndex].editor;
	} else {
		ASSERT(panels.empty());
		return nullptr;
	}
}

const Editor* App::GetFocusedEditor() const {
	return const_cast<App*>(this)->GetFocusedEditor();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void CommandChangeFocusedTab(App* self, bool next) {
	App::Panel& panel = self->panels[self->focusedPanelIndex];
	const u64 startIndex = panel.tabIndex;
	
	u64 i = next
		? IncrementWrapAround(startIndex, self->tabs.size())
		: DecrementWrapAround(startIndex, self->tabs.size());
	
	while (i != startIndex) {
		App::Tab& tab = self->tabs[i];
		if (tab.panelIndex == U64_MAX) {
			ChangeTabOfFocusedPanel(self, i);
			break;
		}
		
		i = next
			? IncrementWrapAround(i, self->tabs.size())
			: DecrementWrapAround(i, self->tabs.size());
	}	
}

static void CommandAddPanel(App* self, bool beforeCurrent) {
	
	App::Panel newPanel {};
	
	// find tab to display	
	for (u64 i = 0; i < self->tabs.size(); i++) {
		App::Tab& tab = self->tabs[i];
		
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
	self->focusedPanelIndex = insertIndex;
	
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

static void ActionSwapPanels(App* self) {

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

static void ActionClosePanel(App* self) {
	if (self->panels.size() <= 1u) return;
	
	self->panels.erase(self->panels.begin() + self->focusedPanelIndex);
	ASSERT(!self->panels.empty());
	
	if (self->focusedPanelIndex == self->panels.size())
		self->focusedPanelIndex--;
	
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

static void CommandCloseTab(App* self) {
	//if (self->tabs.size() <= 1u) return;
	
	App::Panel& panel = self->panels[self->focusedPanelIndex];
	
	// close old tab
	{
		App::Tab& oldTab = self->tabs[panel.tabIndex];
		
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
			App::Tab& tab = self->tabs[i];
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

static void ActionCloseTabAndPanel(App* self) {
	if (self->tabs.size() <= 1u) return;
	
	App::Panel& panel = self->panels[self->focusedPanelIndex];
	
	// close old tab
	{
		App::Tab& oldTab = self->tabs[panel.tabIndex];
		
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

static void CommandNewFile(App* self) {
	// @TODO
}

static void CommandCloseMultipleTabs(App* self, int tabsToClose) {
	if (self->focusedPanelIndex == U64_MAX) return;
	const u64 focusedTab = self->panels[self->focusedPanelIndex].tabIndex;
	
	 // close tabs to the left
	if (tabsToClose < 0) {
		for (u64 i = 0; i < focusedTab; i++) {
			App::Tab& tab = self->tabs[i];
					const auto closeResult = tab.editor->CloseFile();
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return;
			}
			
			delete tab.editor;
			tab.editor = nullptr;
		}
		
		self->tabs.erase(self->tabs.begin(), self->tabs.begin() + focusedTab);
	
	 // close other tabs
	} else if (tabsToClose == 0) {
		for (u64 i = 0; i < self->tabs.size(); i++) {
			if (i == focusedTab) continue;
			
			App::Tab& tab = self->tabs[i];
					const auto closeResult = tab.editor->CloseFile();
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return;
			}
			
			delete tab.editor;
			tab.editor = nullptr;
		}
		
		std::swap(self->tabs.front(), self->tabs[focusedTab]);
		self->tabs.erase(self->tabs.begin() + 1, self->tabs.end());
	
	// close tabs to the right
	} else if (tabsToClose > 0) {
		for (u64 i = focusedTab + 1u; i < self->tabs.size(); i++) {
			App::Tab& tab = self->tabs[i];
					const auto closeResult = tab.editor->CloseFile();
			if (closeResult != Editor::FileResult_Success) {
				LogFileResult(closeResult);
				return;
			}
			
			delete tab.editor;
			tab.editor = nullptr;
		}
		
		self->tabs.erase(self->tabs.begin() + focusedTab + 1u, self->tabs.end());
	}
	
	RelinkPanelsAndTabs(self);
	ResizePanels(self);
}

static void CommandSaveAll(App* self) {
	for (u64 i = 0u; i < self->tabs.size(); i++) {
		self->tabs[i].editor->SaveFile();
	}
}

static void OnFileChanged(App* self, FileChangedEvent* fileChangedEvent) {
	
	char buffer[_MAX_PATH] {0};
	memcpy(buffer, fileChangedEvent->directory, fileChangedEvent->directoryLength);
	buffer[fileChangedEvent->directoryLength] = '\\';
	
	App::Tab* renamedTab = nullptr;
	for (u64 i = 0u; i < fileChangedEvent->recordCount; i++) {
		const FileChangeRecord& record = fileChangedEvent->records[i];
		
		const std::string_view fn {record.filename, record.filenameLength};
		LogInfo("Action %d -> %.*s", static_cast<int>(record.action), SIZE_AND_DATA(fn));
				
		memcpy(buffer + fileChangedEvent->directoryLength + 1u, record.filename, record.filenameLength);
		const std::string_view recordFilepath {buffer, fileChangedEvent->directoryLength + 1u + record.filenameLength};
		
		if (record.action == FileChangeRecord::Action_RenamedOld) {
			for (App::Tab& tab : self->tabs) {
				if (tab.editor->path == recordFilepath) {
					renamedTab = &tab;
					break;
			 	}
		 	}
		 	
		} else if (record.action == FileChangeRecord::Action_RenamedNew && renamedTab) {
			renamedTab->editor->OnFileChange(&record);
			
			const std::string_view newTitle = GetFilenameFromPath(recordFilepath);
			renamedTab->title.Shape(newTitle, settings.fontUi);
			
			renamedTab = nullptr;
		
		} else {
			for (App::Tab& tab : self->tabs) {
				if (tab.editor->path == recordFilepath) {
					tab.editor->OnFileChange(&record);
					break;
			 	}
			}
		}
	}
}

void App::HandleEvent(const Event& event) {
	
	if (event.type == Event::Type_MouseWheel) {
		
		if (searchBar && RectContains(searchBar->area, mouse.x, mouse.y)) {
			searchBar->OnMouseWheel(event.wheelDistance);
			return;
		}
		
		if (toolOutput.isOpen && RectContains(toolOutput.area, mouse.x, mouse.y)) {
			toolOutput.OnMouseWheel(event.wheelDistance);
			return;
		}
		
		for (const App::Panel& panel : panels) {
			if (RectContains(panel.editor->area, mouse.x, mouse.y)) {
				panel.editor->OnMouseWheel(event.wheelDistance);
			}
		}
	
	
	} else if (event.type == Event::Type_Close) {
		for (const App::Tab& tab : tabs) {
			ASSERT_SOFT(tab.editor);
			
			if (tab.editor->isDirty) {
				auto const saveResult = tab.editor->SaveFile();
				if (saveResult == Editor::FileResult_Failure)
					return; // don't close
			}
			
			mainWindow.Destroy();
		}
	
	} else if (event.type == Event::Type_Resize) {
		ResizePanels(this);
	
		toolOutput.OnResize(event.newSize.w, event.newSize.h);
		searchBarFiles.OnResize();
		searchBarTools.OnResize();
		searchBarCommands.OnResize();
			
	} else if (event.type == Event::Type_FileChange) {
		OnFileChanged(this, event.fileChangedEvent);
			
	} else if (searchBar && searchBar->HandleEvent(event)) {
		if (searchBar->shouldClose) {
			searchBar->shouldClose = false;
			searchBar = nullptr;
		}
	
	} else if (explorer && explorer->HandleEvent(event)) {
		if (explorer->shouldClose) {
			delete explorer;
			explorer = nullptr;
		}
	
	} else if (toolOutput.isOpen && toolOutput.HandleEvent(event)) {
		return;
	
	} else if (Editor* editor = GetFocusedEditor(); editor && editor->HandleEvent(event)) {
		return;
	
	} else if (event.type == Event::Type_Command) {
	
		if (event.cmd.id == Command::Id_OpenFileSearch) {
			searchBar = &searchBarFiles;
			searchBar->shouldClose = false;
		} else if (event.cmd.id == Command::Id_OpenToolSearch) {
			searchBar = &searchBarTools;
			searchBar->shouldClose = false;
		} else if (event.cmd.id == Command::Id_OpenCommandSearch) {
			searchBar = &searchBarCommands;
			searchBar->shouldClose = false;
		} else if (event.cmd.id == Command::Id_ToggleToolOutput) {
			toolOutput.isOpen = !toolOutput.isOpen;
		} else if (event.cmd.id == Command::Id_ToggleExplorer) {
			if (explorer) {
				delete explorer;
				explorer = nullptr;
			} else {
				explorer = Explorer::Make();
			}
		} else if (event.cmd.id == Command::Id_FocusNextTab) {
			CommandChangeFocusedTab(this, true);
		} else if (event.cmd.id == Command::Id_FocusPrevTab) {
			CommandChangeFocusedTab(this, false);
		} else if (event.cmd.id == Command::Id_FocusNextPanel) {
			focusedPanelIndex = IncrementWrapAround(focusedPanelIndex, panels.size());
		} else if (event.cmd.id == Command::Id_FocusPrevPanel) {
			focusedPanelIndex = DecrementWrapAround(focusedPanelIndex, panels.size());
		} else if (event.cmd.id == Command::Id_SwapPanels) {
			ActionSwapPanels(this);
		} else if (event.cmd.id == Command::Id_ClosePanel) {
			ActionClosePanel(this);
		} else if (event.cmd.id == Command::Id_AddPanelAfter) {
			CommandAddPanel(this, false);
		} else if (event.cmd.id == Command::Id_AddPanelBefore) {
			CommandAddPanel(this, true);
		} else if (event.cmd.id == Command::Id_NewFile) {
			CommandNewFile(this);
		} else if (event.cmd.id == Command::Id_CloseFile) {
			CommandCloseTab(this);
		} else if (event.cmd.id == Command::Id_CloseFilesToTheRight) {
			CommandCloseMultipleTabs(this, -1);
		} else if (event.cmd.id == Command::Id_CloseFilesToTheLeft) {
			CommandCloseMultipleTabs(this, 1);
		} else if (event.cmd.id == Command::Id_CloseOtherFiles) {
			CommandCloseMultipleTabs(this, 0);
		} else if (event.cmd.id == Command::Id_ClosePanelAndFile) {
			ActionCloseTabAndPanel(this);
		} else if (event.cmd.id == Command::Id_SaveAll) {
			CommandSaveAll(this);
		}
	}
}


void App::PullEvent() {
	const Event& event = mainWindow.event;
	if (Command command; settings.LookupKeyBind(event, &command)) {
		HandleEvent(Event {
			.type = Event::Type_Command,
			.cmd = command});
	} else {
		HandleEvent(event);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

App::OpenBehavior OpenBehaviorFromModifiers(u32 kmods) {
	if      (kmods == KM_Ctrl)             return App::OpenBehavior_UpdateCurrent;
	else if (kmods == KM_Shift)            return App::OpenBehavior_NewPanelRight;
	else if (kmods == (KM_Shift | KM_Alt)) return App::OpenBehavior_NewPanelLeft;
	return App::OpenBehavior::OpenBehavior_Default;
}
