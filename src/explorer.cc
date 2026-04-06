#include "explorer.hh"
#include "main-window.hh"
#include "globals.hh"
#include "settings.hh"

#include "util/file-util.hh"
#include "util/rect-util.hh"
#include "util/string-util.hh"
#include "util/logging.hh"

#include "ui/constants.h"
#include "graphics/effects.hh"
#include "graphics/glyph-run.hh"

#include <string>

#define WIN32_LEAN_AND_MEAN
#define STRICT_TYPED_ITEMIDS // we want the defines for PIDLIST_ABSOLUTE and such
#include <Shlobj.h>
#include <Shellapi.h>
#include <comdef.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
constexpr float HIGHLIGHT_ANIMATION_MAX_VALUE = F32_PI;
constexpr float HIGHLIGHT_ANIMATION_SPEED = 0.006f;

constexpr float TEXTBOX_PANEL_WIDTH = 250.0f;

constexpr float COPY_ANIMATION_MAX = F32_PI;
constexpr float COPY_ANIMATION_SPEED = 0.006f;

constexpr float INSERT_ANIMATION_MAX = 1.0f;
constexpr float INSERT_ANIMATION_SPEED = 0.002f;

constexpr float ACTIVE_ITEM_ANIMATION_MAX = (F32_PI * 2.0f) * 10.0f; // 10 cycles
constexpr float ACTIVE_ITEM_ANIMATION_SPEED = 0.004f;

static const char* LAYER = "Explorer";

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Helper
//

static std::string BuildPanelPath(const Explorer::Panel* panel, std::string_view appendix = {}) {
	
	std::string path;
	if (!appendix.empty()) {
		path.reserve(panel->fullPathLength + 1 + appendix.length());
		path.push_back('\\');
		path.append(appendix);
	} else {
		path.reserve(panel->fullPathLength);
	}
	
	const Explorer::Panel* currentPanel = panel;
	while (true) {
		path.insert(path.begin(), currentPanel->directoryName.begin(), currentPanel->directoryName.end());	
		
		currentPanel = currentPanel->parent;
		if (!currentPanel) break;
			
		path.insert(path.begin(), '\\');
	}
	
	return path;	
}

static bool RefreshPanelItems(Explorer::Panel* panel, const std::string& directoryPath, bool flagNewItemsAsPasted) {
	
	// save some stuff so it doesn't get overriden
	const u64 oldActiveItemIndex = panel->items.empty() ? 0u : (panel->activeItem - panel->items.data());
	std::vector<Explorer::Item> oldItems = std::move(panel->items);
	const Explorer::Item* oldActiveItem = panel->activeItem;
	
	//
	// add items
	//
	{
		u64 newActiveItemIndex = U64_MAX;
		
		DirectoryIterator dirIt {directoryPath};
		while (dirIt.Next()) {
			Explorer::Item::Type type = dirIt.IsDirectory()
					? Explorer::Item::Type_Directory
					: Explorer::Item::Type_File;
				
			for (Explorer::Item& oldItem : oldItems) {
				if (oldItem.filename == dirIt.filename &&
				    oldItem.type == type) {
					
					if (&oldItem == oldActiveItem)
						newActiveItemIndex = panel->items.size();
						
					panel->items.push_back(std::move(oldItem));
					
					goto found_item;
				}
			}
			
			panel->items.push_back(Explorer::Item {
				.type = type,
				.isSelected = false,
				.flags = static_cast<u32>(flagNewItemsAsPasted ? Explorer::Item::Flag_Inserted : Explorer::Item::Flag_None),
				.filename = dirIt.filename});
			
		found_item:
			continue;
		}
		
		if (panel->items.empty()) {
			panel->items.push_back(Explorer::Item {
				.type = Explorer::Item::Type_Placeholder,
				.filename = "No Items"});
		}
		
		if (newActiveItemIndex == U64_MAX)
			newActiveItemIndex = std::min(oldActiveItemIndex, panel->items.size() - 1u);
		
		ASSERT(newActiveItemIndex < panel->items.size());
		panel->activeItem = &panel->items[newActiveItemIndex];
		
		if (dirIt.Failed()) {
			panel->activeItem = &panel->items.front();
			LogError("failed to read directory '%'", directoryPath);
			return false;
		}
	}
	
	//
	// calc panel size
	//
	{
		auto itMaxItem = std::max_element(panel->items.begin(), panel->items.end(),
			[] (const Explorer::Item& lhs, const Explorer::Item& rhs) { return lhs.filename.size() < rhs.filename.size(); });
		
		ASSERT(itMaxItem != panel->items.end());
		
		staticGlyphRun.Shape(itMaxItem->filename, settings.fontUi);
		const float textWidth = staticGlyphRun.width;
		
  		const float width  = MARGIN_X2 + settings.fontUi.lineHeight + textWidth + MARGIN;
		const float height = panel->items.size() * settings.fontUi.lineHeight;
		
		panel->area.right = panel->area.left + width;
		panel->area.bottom = panel->area.top + height;	
	}
	
	return true;
}

static Explorer::NewItemDialog* CreateTextboxPanel(D2D_POINT_2F spawnPoint, std::string_view placeholderText, std::string initialText) {
	auto tbPanel = std::make_unique<Explorer::NewItemDialog>();
		
	if (!tbPanel->textbox.Init(&settings.fontUi, placeholderText, std::move(initialText)))
		return nullptr;
	
	tbPanel->textbox.position = D2D_POINT_2F {
		.x = spawnPoint.x + MARGIN,
		.y = spawnPoint.y + MARGIN + settings.fontUi.lineHeight + MARGIN};
	tbPanel->textbox.width = TEXTBOX_PANEL_WIDTH - MARGIN_X2;
	
	tbPanel->area = D2D_RECT_F {
		.left   = spawnPoint.x,
		.top    = spawnPoint.y,
		.right  = spawnPoint.x + TEXTBOX_PANEL_WIDTH,
		.bottom = spawnPoint.y + MARGIN + settings.fontUi.lineHeight + MARGIN + tbPanel->textbox.Height() + MARGIN};
	
	return tbPanel.release();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Explorer* Explorer::Make() {
	
	auto self = std::make_unique<Explorer>();

	auto panel = std::make_unique<Explorer::Panel>();	
	panel->area.left = 0u;
	panel->area.top = settings.fontUi.lineHeight + PADDING_X2;
	panel->parent = nullptr;
	panel->directoryName = ".";
	panel->fullPathLength = 1u;
	
	if (!RefreshPanelItems(panel.get(), panel->directoryName, false))
		return nullptr;
	
	self->activePanel = panel.release();
		
	return self.release();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Actions
//

// Copy the full filepath of every selected item to the buffer.
// Each filename is delimited by a \0 and the whole sequence is terminated by a final \0  just like functions
// like SHFileOperation and such require.
static u64 CopyFilenamesOfSelectedItems(const Explorer::Panel* panel, std::span<char> buffer) {	
	
	const std::string directoryPath = BuildPanelPath(panel);
	
	if (buffer.empty()) {
		u64 requiredSize = 0u;
		for (const Explorer::Item& item : panel->items) {
			if (&item == panel->activeItem || item.isSelected) {
			 	// +1 for the \ after the directory path
			 	// another +1 for the seperating \0
				requiredSize += directoryPath.size() + item.filename.size() + 2;
			}
		}
		
		// for the final \0
		requiredSize += 1;
		return requiredSize;
	
	} else {
		for (const Explorer::Item& item : panel->items) {
			if (&item == panel->activeItem || item.isSelected) {
				std::copy(directoryPath.begin(), directoryPath.end(), buffer.begin());
			 	buffer = buffer.subspan(directoryPath.size());
			 	
			 	buffer.front() = '\\';
			 	buffer = buffer.subspan(1);
				
			 	std::copy(item.filename.begin(), item.filename.end(), buffer.begin());
			 	buffer = buffer.subspan(item.filename.size());
				
			 	buffer.front() = '\0';
				buffer = buffer.subspan(1);
			}
		}
			 
		// final \0
		buffer.front() = '\0';
		buffer = buffer.subspan(1);
		
		return 0;
	}
}

static void ActionCopyOrCut(Explorer* self, bool cut) {
	// see: https://stackoverflow.com/questions/25708895/how-to-copy-files-by-win32-api-functions-and-paste-by-ctrlv-in-my-desktop
	
	const u64 requiredSizeFilenames = CopyFilenamesOfSelectedItems(self->activePanel, {});
	const u64 requiredSize = sizeof(DROPFILES) + requiredSizeFilenames;
			
	HGLOBAL hGlobalDropfiles = GlobalAlloc(GMEM_MOVEABLE, requiredSize);
		
	//
	// copy the filepaths
	//
	{
		auto dropfiles = static_cast<DROPFILES*>(GlobalLock(hGlobalDropfiles));
		dropfiles->pFiles = sizeof(DROPFILES);
		dropfiles->fWide = FALSE;
			
		std::span<char> filesBuffer {reinterpret_cast<char*>(dropfiles + 1), requiredSizeFilenames};
		CopyFilenamesOfSelectedItems(self->activePanel, filesBuffer);
		
		GlobalUnlock(hGlobalDropfiles);
	}
		
	HGLOBAL hGlobalDropEffect = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
	const UINT cfDropEffect = RegisterClipboardFormatA(CFSTR_PREFERREDDROPEFFECT);
		
	//
	// set the drop effect to DROPEFFECT_COPY to indicate that the files are copied (not cut)
	//
	{
		auto dropEffect = static_cast<DWORD*>(GlobalLock(hGlobalDropEffect));
		*dropEffect = cut ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
		GlobalUnlock(hGlobalDropEffect);
	}

	if (!OpenClipboard(mainWindow.hWnd)) {
		LogError("OpenClipboard() failed. Last Error: %", FLastErr(GetLastError()));
		GlobalFree(hGlobalDropfiles);
		GlobalFree(hGlobalDropEffect);
		return;
	}
		
	EmptyClipboard();
	SetClipboardData(CF_HDROP, hGlobalDropfiles);
	SetClipboardData(cfDropEffect, hGlobalDropEffect);
	CloseClipboard();
		
	//
	// start animation
	//
	{
		for (Explorer::Item& item : self->activePanel->items) {
			if (item.isSelected || &item == self->activePanel->activeItem) {
				item.flags |= Explorer::Item::Flag_Copied;
				if (cut) item.flags |= Explorer::Item::Flag_Cut;
			}
		}
		
		self->copyAnimationRunning = true;
		self->copyAnimationValue = 0.0f;
	}
}

static void ActionPaste(Explorer* self) {
	
	if (!IsClipboardFormatAvailable(CF_HDROP)) {
		LogWarning("No clipboard data or clipboard format no compatible");
		return;
	}
		
	if (!OpenClipboard(mainWindow.hWnd)) {
		LogError("OpenClipboard() failed. Last Error: %", FLastErr(GetLastError()));
		return;
	}
		
	DEFER(CloseClipboard());
	
	//
	// get paste flavor (cut or copy)
	//	
	UINT operation = FO_COPY;
	{
		const UINT cfDropEffect = RegisterClipboardFormatA(CFSTR_PREFERREDDROPEFFECT);		
		if (IsClipboardFormatAvailable(cfDropEffect)) {
			HANDLE hDropEffect = GetClipboardData(cfDropEffect);
				
			if (hDropEffect != NULL) {
				auto preferedDropEffect = static_cast<const DWORD*>(GlobalLock(hDropEffect));
				DEFER(GlobalUnlock(hDropEffect));
					
				if (!preferedDropEffect) {
					LogWarning("prefered dropeffect is null");
					operation = FO_COPY;
						
				} else if  (*preferedDropEffect == DROPEFFECT_NONE || *preferedDropEffect == DROPEFFECT_COPY) {
					operation = FO_COPY;
						
				} else if (*preferedDropEffect == DROPEFFECT_MOVE) {
					operation = FO_MOVE;
						
				} else {
					LogError("Unrecognized prefered dropeffect: %", preferedDropEffect);
					return;
				}
					
			} else {
				LogWarning("GetClipboardData() failed. Last Error: %", FLastErr(GetLastError()));
			}
		}
	}
	
	//
	// do the shell operator
	//
	HANDLE hDrop = GetClipboardData(CF_HDROP);
	if (hDrop == NULL) {
		LogError("GetClipboardData() failed. Last Error: %", FLastErr(GetLastError()));
		return;
	}
	
	const u8* memory = static_cast<const u8*>(GlobalLock(hDrop));
	if (!memory) {
		LogError("GlobalLock() returned null: %", FLastErr(GetLastError()));
		return;
	}
	
	auto dropfiles = reinterpret_cast<const DROPFILES*>(memory);
	
	const std::string directoryPath = BuildPanelPath(self->activePanel);
		
	int result;
	if (!dropfiles->fWide) {
		auto filelist = reinterpret_cast<const char*>(memory + dropfiles->pFiles);
		SHFILEOPSTRUCTA fileOperation {
			.hwnd = mainWindow.hWnd,
			.wFunc = operation,
			.pFrom = filelist,
			.pTo = directoryPath.c_str(),
			.fFlags = FOF_ALLOWUNDO | FOF_WANTNUKEWARNING};
		
		result = SHFileOperationA(&fileOperation);
					
	} else {
		// convert to wide
		wchar wtargetPath[_MAX_PATH + 1];
		u64 wtargetPathLen = 0;
		ToUtf16(directoryPath, std::span<wchar>{wtargetPath, _MAX_PATH}, &wtargetPathLen);
		wtargetPath[wtargetPathLen] = '\0';
					
		auto filelist = reinterpret_cast<const wchar*>(memory + dropfiles->pFiles);
		SHFILEOPSTRUCTW fileOperation {
			.hwnd = mainWindow.hWnd,
			.wFunc = operation,
			.pFrom = filelist,
			.pTo = wtargetPath,
			.fFlags = FOF_ALLOWUNDO | FOF_WANTNUKEWARNING};
		result = SHFileOperationW(&fileOperation);
	}
		
	if (result != 0) {
		// @TODO error handling could be improved
		// see: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shfileoperationa
		LogError("SHFileOperation failed. Return code: %", FLastErr(result));
	}
	
	//
	// update the panel and start the highlight animation
	//
	{
		if (!RefreshPanelItems(self->activePanel, directoryPath, true)) {
			LogError("UpdatePanel failed");
			return;
		}
		
		self->insertAnimationRunning = true;
		self->insertAnimationValue = 0.0f;
	}
}

static void ActionDelete(Explorer* self) {
		
	const u64 requiredSize = CopyFilenamesOfSelectedItems(self->activePanel, {});
	
	char* filelist = new char[requiredSize];
	CopyFilenamesOfSelectedItems(self->activePanel, {filelist, requiredSize});
	
	SHFILEOPSTRUCTA fileOperation {
		.hwnd = mainWindow.hWnd,
		.wFunc = FO_DELETE,
		.pFrom = filelist,
		.pTo = nullptr,
		.fFlags = FOF_ALLOWUNDO | FOF_WANTNUKEWARNING};
		
	const int result = SHFileOperationA(&fileOperation);
	if (result != 0) {
		// @TODO error handling could be improved
		// see: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shfileoperationa
		LogError("SHFileOperation failed. Return code: %", FLastErr(result));
	}
	
	// @TODO: we calculate it twice: here and in CopyFilenamesOfSelectedItems
	const std::string directoryPath = BuildPanelPath(self->activePanel);
	
	if (!RefreshPanelItems(self->activePanel, directoryPath, false)) {
		LogError("UpdatePanel failed");
		return;
	}
}

static void ActionShellExecute(Explorer* self) {
	
	const std::string itemPath = BuildPanelPath(self->activePanel, self->activePanel->activeItem->filename);
	const auto result = reinterpret_cast<INT_PTR>(ShellExecuteA(mainWindow.hWnd, NULL, itemPath.c_str(), NULL, NULL, SW_SHOW));
	
	// see: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutea
	if (result <= 32) {
		
		std::string_view errorMsg;
		switch (result) {
			case 0: errorMsg = "The operating system is out of memory or resources."; break;
			case ERROR_BAD_FORMAT: errorMsg = "The .exe file is invalid (non-Win32 .exe or error in .exe image)."; break;
			case SE_ERR_FNF: errorMsg = "The specified file was not found."; break; // same as ERROR_FILE_NOT_FOUND
			case SE_ERR_PNF: errorMsg = "The specified path was not found."; break; // same as ERROR_PATH_NOT_FOUND
			case SE_ERR_ACCESSDENIED: errorMsg = "The operating system denied access to the specified file."; break;
			case SE_ERR_ASSOCINCOMPLETE: errorMsg = "The file name association is incomplete or invalid."; break;
			case SE_ERR_DDEBUSY: errorMsg = "The DDE transaction could not be completed because other DDE transactions were being processed."; break;
			case SE_ERR_DDEFAIL: errorMsg = "The DDE transaction failed."; break;
			case SE_ERR_DDETIMEOUT: errorMsg = "The DDE transaction could not be completed because the request timed out."; break;
			case SE_ERR_DLLNOTFOUND: errorMsg = "The specified DLL was not found."; break;	
			case SE_ERR_NOASSOC: errorMsg = "There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable."; break;
			case SE_ERR_OOM: errorMsg = "There was not enough memory to complete the operation."; break;
			case SE_ERR_SHARE: errorMsg = "A sharing violation occurred. "; break;
			default: errorMsg = "(unknown)"; break;
		}
		
		LogError("ShellExecuteA() failed: % %. Last Error: %", result, errorMsg, FLastErr(GetLastError()));
	}
}

static void ActionRevealInExplorer(Explorer* self) {

	PIDLIST_ABSOLUTE idlistBase = nullptr;
	
	//
	// get the idlist of the current directory
	//
	{
		const std::string directoryPath = BuildPanelPath(self->activePanel);
	
		u64 length = 0u;
		wchar buffer[_MAX_PATH + 1]; // need to \0-terminate
		
		if (!ToUtf16(directoryPath, buffer, &length)) {
			LogError("failed to convert base path to utf16");
			return;
		}
		
		buffer[length] = '\0';
		
		if (HRESULT hr = SHParseDisplayName(buffer, nullptr, &idlistBase, 0, nullptr); hr != S_OK) {
			LogError("SHParseDisplayName() failed. HRESULT: %", FHr(hr));
			return;
		}
	}
	
	//
	// create the IShellFolder
	//
	IShellFolder* shellFolder = nullptr;
	if (HRESULT hr = SHBindToObject(
			nullptr,
			static_cast<PCIDLIST_RELATIVE>(idlistBase),
			nullptr, 
			__uuidof(IShellFolder),
			reinterpret_cast<void**>(&shellFolder)); hr != S_OK) {
		LogError("SHBindToObject failed. HRESULT: %", FHr(hr));
		return;
	}
	DEFER(shellFolder->Release());
	
	//
	// create the Enumerator for the child items
	//
	IEnumIDList* enumIdList = nullptr;
	if (HRESULT hr = shellFolder->EnumObjects(mainWindow.hWnd, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &enumIdList); hr != S_OK) {
		LogError("IShellFolder::EnumObjects failed. HRESULT: %", hr);
		return;
	}
	DEFER(enumIdList->Release());
	
	//
	// prepare the vector of items to select
	//
	std::vector<ITEMID_CHILD*> itemsToSelect;
	{
		u64 numItemsToSelect = 0;
		for (const Explorer::Item& item : self->activePanel->items) {
			if (item.isSelected || &item == self->activePanel->activeItem)
				numItemsToSelect++;
		}
		
		itemsToSelect.reserve(numItemsToSelect);
	}
	
	//
	// the childItemIds of the items to select
	//
	for (auto childItemId = static_cast<ITEMID_CHILD*>(CoTaskMemAlloc(sizeof(ITEMID_CHILD)));
		 enumIdList->Next(1, &childItemId, nullptr) == S_OK;
		 CoTaskMemFree(childItemId)) {
		
		STRRET strret;
		if (HRESULT hr = shellFolder->GetDisplayNameOf(childItemId, SHGDN_INFOLDER | SHGDN_FORPARSING, &strret); hr != S_OK) {
			LogError("IShellFolder::GetDisplayNameOf failed. HRESULT: %", hr);
			continue;
		}
			
		switch (strret.uType) {
			case STRRET_CSTR: {
				std::string_view childName {strret.cStr};
				for (const Explorer::Item& item : self->activePanel->items) {
					if (item.filename == childName) {
						if (item.isSelected || &item == self->activePanel->activeItem) {
							itemsToSelect.push_back(childItemId);
							childItemId = nullptr;
						}
						break;
					}
				}
			} break;
			
			case STRRET_OFFSET:
			case STRRET_WSTR: {
				const wchar* childName = nullptr;
				if (strret.uType == STRRET_OFFSET) {
					childName = reinterpret_cast<const wchar*>(
						reinterpret_cast<const u8*>(childItemId) + strret.uOffset);
				} else {
					childName = strret.pOleStr;
				}
				
				for (const Explorer::Item& item : self->activePanel->items) {
					if (StringEquals(item.filename, std::wstring_view {childName})) {
						if (item.isSelected || &item == self->activePanel->activeItem) {
							itemsToSelect.push_back(childItemId);
							childItemId = nullptr;
						}
						break;
					}
				}
				
				if (strret.uType == STRRET_WSTR)
					CoTaskMemFree(strret.pOleStr);
			} break;
			
			default: {
				LogError("Unknown uType in STRRET: %", strret.uType);
			} break;
		}
	}
	DEFER(
		for (ITEMID_CHILD* itemid : itemsToSelect)
			CoTaskMemFree(itemid)
	);
	
	//
	// open the windows explorer
	//
	if (HRESULT hr = SHOpenFolderAndSelectItems(idlistBase, static_cast<UINT>(itemsToSelect.size()), itemsToSelect.data(), 0); hr != S_OK) {
		LogError("SHOpenFolderAndSelectItems failed. HRESULT: %", hr);
		return;
	}
}

static void ActionNewItem(Explorer* self, Explorer::Item::Type type) {
	ASSERT(!self->newItemDialog);
	ASSERT(type == Explorer::Item::Type_File || type == Explorer::Item::Type_Directory);
	
	const u64 activeItemIndex = self->activePanel->activeItem - self->activePanel->items.data();
	self->newItemDialog = CreateTextboxPanel(
		D2D_POINT_2F {
			.x = self->activePanel->area.right,
			.y = self->activePanel->area.top + (activeItemIndex * settings.fontUi.lineHeight)},
		type == Explorer::Item::Type_File ? "File name" : "Folder name",
		std::string {});
	self->newItemDialog->itemType = type;
	self->newItemDialog->isRename = false;
}

static void ActionRenameItem(Explorer* self) {
	ASSERT(!self->newItemDialog);
	
	const u64 activeItemIndex = self->activePanel->activeItem - self->activePanel->items.data();
	self->newItemDialog = CreateTextboxPanel(
		D2D_POINT_2F {
			.x = self->activePanel->area.right,
			.y = self->activePanel->area.top + (activeItemIndex * settings.fontUi.lineHeight)},
		self->activePanel->activeItem->type  == Explorer::Item::Type_File ? "Rename file" : "Rename folder",
		self->activePanel->activeItem->filename);
	self->newItemDialog->itemType = self->activePanel->activeItem->type;
	self->newItemDialog->isRename = true;
}

static void ActionConfirmTextboxPanel(Explorer* self) {
	ASSERT(self->newItemDialog);
	
	const std::string directoryPath = BuildPanelPath(self->activePanel);
	const std::string targetPath = FormatString("%\\%", directoryPath, self->newItemDialog->textbox.GetText());
	
	if (self->newItemDialog->isRename) {
		
		const std::string sourcePath = FormatString("%\\%", directoryPath, self->activePanel->activeItem->filename);
		
		SHFILEOPSTRUCTA fileOperation {
			.hwnd = mainWindow.hWnd,
			.wFunc = FO_RENAME,
			.pFrom = sourcePath.c_str(),
			.pTo = targetPath.c_str(),
			.fFlags = FOF_ALLOWUNDO};
		
		const int result = SHFileOperationA(&fileOperation);
		if (result != 0) {
			// @TODO error handling could be improved
			// see: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shfileoperationa
			LogError("SHFileOperation failed. Return code: %", FLastErr(result));
		}
			
	} else {
		if (self->newItemDialog->itemType == Explorer::Item::Type_File) {
	
			HANDLE hFile = CreateFileA(
				targetPath.c_str(),
				GENERIC_WRITE,
				0,
				nullptr,
				CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
				
			if (hFile == INVALID_HANDLE_VALUE) {
				
				const unsigned int lastErr = GetLastError();
				LogWarning("CreateFileA failed: %", FLastErr(lastErr));
				
				const _com_error comError {HRESULT_FROM_WIN32(lastErr)};
				
				self->newItemDialog->errorText = comError.ErrorMessage();
				self->newItemDialog->area.bottom += MARGIN + settings.fontUi.lineHeight;
				self->newItemDialog->textbox.invalid = true;
				return;
			}
			
			CloseHandle(hFile);	
		
		} else if (self->newItemDialog->itemType == Explorer::Item::Type_Directory)  {
			
			const BOOL ok = CreateDirectoryA(targetPath.c_str(), NULL);
			
			if (!ok) {
				const unsigned int lastErr = GetLastError();
				LogWarning("CreateDirectoryA failed: %", FLastErr(lastErr));
				
				const _com_error comError {HRESULT_FROM_WIN32(lastErr)};
				
				self->newItemDialog->errorText = comError.ErrorMessage();
				self->newItemDialog->area.bottom += MARGIN + settings.fontUi.lineHeight;
				self->newItemDialog->textbox.invalid = true;
				return;
			}
		
		} else {
			ASSERT_UNREACHABLE
		}
	}
	
	delete self->newItemDialog;
	self->newItemDialog = nullptr;
	RefreshPanelItems(self->activePanel, directoryPath, true);
	self->insertAnimationRunning = true;
	self->insertAnimationValue = 0.0f;
}

static void ActionOpenDirectory(Explorer* self) {
	ASSERT(self->activePanel->activeItem->type == Explorer::Item::Type_Directory);
	
	self->activeItemAnimationValue = 0.0f;
	
	const u64 activeItemIndex = (self->activePanel->activeItem - self->activePanel->items.data());
	const D2D_POINT_2F spawnPos {
		.x = self->activePanel->area.right,
		.y = self->activePanel->area.top + (activeItemIndex * settings.fontUi.lineHeight)};
			
	auto newPanel = std::make_unique<Explorer::Panel>();	
	newPanel->area.left = spawnPos.x;
	newPanel->area.top  = spawnPos.y;
	newPanel->parent = self->activePanel;
	newPanel->directoryName = self->activePanel->activeItem->filename;
	newPanel->fullPathLength = self->activePanel->fullPathLength + 1u + self->activePanel->activeItem->filename.length();
	
	const std::string directoryPath = BuildPanelPath(self->activePanel, self->activePanel->activeItem->filename);
	if (!RefreshPanelItems(newPanel.get(), directoryPath, false))
		return;
	
	self->activePanel = newPanel.release();	
}

static void ActionOpenItem(Explorer* self, MainWindow::OpenBehavior openBehav) {
	
	if (self->activePanel->activeItem->type == Explorer::Item::Type_File) {
		const std::string path = BuildPanelPath(self->activePanel, self->activePanel->activeItem->filename);

		self->shouldClose = mainWindow.OpenEditor(path, openBehav);
		
	} else if (self->activePanel->activeItem->type == Explorer::Item::Type_Directory) {
		ActionOpenDirectory(self);
		self->shouldClose = false;
	}
}

static void ActionClick(Explorer* self, Explorer::Panel* clickedPanel, Explorer::Item* clickedItem) {
	ASSERT(clickedItem >= clickedPanel->items.data() && clickedItem < clickedPanel->items.data() + clickedPanel->items.size());
	
	self->insertAnimationValue = 0.0f;
	
	if (self->activePanel == clickedPanel) {
		clickedPanel->activeItem = clickedItem;
		ActionOpenItem(self, MainWindow::OpenBehavior_Default);
	
	} else {
		Explorer::Panel* panel = self->activePanel;
		while (panel != clickedPanel) {
			Explorer::Panel* closingPanel = panel;
			panel = panel->parent;
			delete closingPanel;
		}
		
		self->activePanel = clickedPanel;
		clickedPanel->activeItem = clickedItem;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Update
//

static void OnUpdatePanel(Explorer* self, Explorer::Panel* panel) {
	const bool isActivePanel = (self->activePanel == panel);
	
	//
	// draw background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, panel->area);
		if (!background) return;
		DEFER(background->Release());
					
		if (isActivePanel && !self->newItemDialog)
			DrawGlow(deviceContext, background, panel->area);
		
		PushLayer(deviceContext, panel->area);
		BlurArea(deviceContext, panel->area, background);
	}
	
	DEFER(PopLayer(deviceContext));
	
	//
	// update items
	//
	for (u64 iItem = 0u; iItem < panel->items.size(); iItem++) {
		Explorer::Item& item = panel->items[iItem];
		
		const D2D_RECT_F itemArea {
			.left   = panel->area.left,
			.top    = panel->area.top  + (iItem * (settings.fontUi.lineHeight)),
			.right  = panel->area.right,
			.bottom = panel->area.top  + (iItem * (settings.fontUi.lineHeight)) + settings.fontUi.lineHeight};
				
		if (&item == panel->activeItem) {
			const bool isActiveItemOrRenamed = (isActivePanel && (!self->newItemDialog || self->newItemDialog->isRename));
			
			ID2D1Brush* brush = nullptr;
			if (isActiveItemOrRenamed) {
				brush = settings.GetBrushDropShadow();
				brush->SetOpacity(std::sin(self->activeItemAnimationValue) * 0.4f + 0.5f);
				DEFER(brush->SetOpacity(1.0));
				
				deviceContext->FillRectangle(itemArea, brush);
			
			} else {
				deviceContext->FillRectangle(itemArea, settings.GetBrushUiBackground());
			}
		}
		
		//
		// handle flags
		//
		{
			if (item.isSelected)
				deviceContext->FillRoundedRectangle(MakeRoundedRect(itemArea, RADIUS), settings.GetBrushSelection());
				
			if (item.flags & Explorer::Item::Flag_Inserted) {
				ID2D1SolidColorBrush* insertAnimBrush = settings.GetBrushSelection();
				insertAnimBrush->SetOpacity(1.0f - self->insertAnimationValue);
				DEFER(insertAnimBrush->SetOpacity(1.0f));
				
				deviceContext->FillRectangle(itemArea, insertAnimBrush);
				
				if (!self->insertAnimationRunning)
					item.flags &= ~Explorer::Item::Flag_Inserted;
			}
			
			if (item.flags & Explorer::Item::Flag_Copied) {
				ID2D1SolidColorBrush* copyAnimBrush = settings.GetBrushSelection();
				copyAnimBrush->SetOpacity(std::sin(self->copyAnimationValue));
				DEFER(copyAnimBrush->SetOpacity(1.0f));
				
				deviceContext->FillRectangle(itemArea, copyAnimBrush);
				
				if (!self->copyAnimationRunning)
					item.flags &= ~Explorer::Item::Flag_Copied;
			}
		}
		
		//
		// icon + text
		//
		{
			ID2D1Bitmap* icon = settings.icons.unknown;
			if (item.type == Explorer::Item::Type_Directory) {
				icon = (&item == panel->activeItem && !isActivePanel)
			 		? settings.icons.explorerFolderOpen
					: settings.icons.explorerFolderClosed;
						
			} else if (item.type == Explorer::Item::Type_File) {
				icon = settings.icons.explorerFile;
			
			} else {
				icon = settings.icons.noItems;
			}
			
			deviceContext->DrawBitmap(icon,
				D2D_RECT_F {
					.left   = panel->area.left + MARGIN,
					.top    = itemArea.top,
					.right  = panel->area.left + MARGIN + settings.fontUi.lineHeight,
					.bottom = itemArea.bottom});
			
			const bool isCut = (item.flags & Explorer::Item::Flag_Cut);
			staticGlyphRun.Shape(item.filename, settings.fontUi);
			staticGlyphRun.Draw(deviceContext,
				panel->area.left + MARGIN + settings.fontUi.lineHeight + MARGIN,
				itemArea.top,
				settings.fontUi,
				settings.GetBrushUiText(!isCut));
		}
		
		//
		// handle click
		//
		// @TODO(mouse)
		/*
		if (mouse.Hittest("Explorer", itemArea)) {
			deviceContext->FillRectangle(itemArea, style.GetBrushHover());
			
			if (mouse.IsClicked()) {
				ActionClick(self, panel, &item);
			}
		}*/
	}
}

void Explorer::OnUpdate() {

	// advance animations
	{
		if (insertAnimationRunning) {
			insertAnimationValue += INSERT_ANIMATION_SPEED * deltaTime;
			if (insertAnimationValue > INSERT_ANIMATION_MAX) {
				insertAnimationValue = INSERT_ANIMATION_MAX;
				insertAnimationRunning = false;
			} else needsUpdate = true;
		}
		
		if (copyAnimationRunning) {
			copyAnimationValue += COPY_ANIMATION_SPEED * deltaTime;
			if (copyAnimationValue >= COPY_ANIMATION_MAX) {
				copyAnimationValue =  COPY_ANIMATION_MAX;
				copyAnimationRunning = false;
			} else needsUpdate = true;
		}
		
		activeItemAnimationValue += ACTIVE_ITEM_ANIMATION_SPEED * deltaTime;
		if (activeItemAnimationValue > ACTIVE_ITEM_ANIMATION_MAX)
			activeItemAnimationValue = ACTIVE_ITEM_ANIMATION_MAX;
		else needsUpdate = true;
	}
		
	// panels
	{
		for (Panel* panel = activePanel->parent; panel != nullptr; panel = panel->parent)
			OnUpdatePanel(this, panel);
		
		OnUpdatePanel(this, activePanel);
	}
	
	// new item panel
	if (newItemDialog) {
		
		std::string_view labelText;
		if (newItemDialog->isRename) {
			labelText = newItemDialog->itemType == Item::Type_File
				? "Rename File:"
				: "Rename Folder:";
		} else {
			labelText = newItemDialog->itemType == Item::Type_File
				? "New File:"
				: "New Folder:";
		}
		
		{
			ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, newItemDialog->area);
			if (!background) return;
			DEFER(background->Release());
			
			DrawGlow(deviceContext, background, newItemDialog->area);
			
			PushLayer(deviceContext, newItemDialog->area);
			BlurArea(deviceContext, newItemDialog->area, background);
			PopLayer(deviceContext);
		}
		
		staticGlyphRun.ShapeAndDraw(deviceContext,
			labelText,
			newItemDialog->area.left + MARGIN,
			newItemDialog->area.top + MARGIN,
			settings.fontUi,
			settings.GetBrushUiText());
		
		newItemDialog->textbox.OnUpdate();
		
		if (!newItemDialog->errorText.empty()) {
			staticGlyphRun.ShapeAndDraw(deviceContext,
				newItemDialog->errorText,
				newItemDialog->area.left + MARGIN,
				newItemDialog->textbox.position.y + newItemDialog->textbox.Height() + MARGIN,
				settings.fontUi,
				settings.GetBrushUiText());
		}
	}
}


void Explorer::OnKeyDown(KeyEvent event) {
	
	if (newItemDialog) {
		if (event.vkeycode == VK_ESCAPE || event.vkeycode == VK_RIGHT) {
			delete newItemDialog;
			newItemDialog = nullptr;
		
		} else if (event.vkeycode == VK_RETURN) {
			ActionConfirmTextboxPanel(this);
		
		} else {
			newItemDialog->textbox.OnKeyDown(event);
		}
	
	} else {
	
		if (event.vkeycode == VK_DOWN || event.vkeycode == VK_UP) {
					
			Item* oldActiveItem = activePanel->activeItem;

			const u64 oldActiveItemIndex = activePanel->activeItem - activePanel->items.data();
			const u64 newActiveItemIndex = event.vkeycode == VK_DOWN ?
				IncrementWrapAround(oldActiveItemIndex, activePanel->items.size()):
				DecrementWrapAround(oldActiveItemIndex, activePanel->items.size());
				
			activePanel->activeItem = &activePanel->items[newActiveItemIndex];
				
			if (event.shift) {				
				if (activePanel->activeItem->isSelected)
					activePanel->activeItem->isSelected = false;
				else
					oldActiveItem->isSelected = true;
			
			} else {
				// reset selection
				for (Item& item : activePanel->items)
					item.isSelected = false;
			}
			
			activeItemAnimationValue = 0.0f;
				
		} else if (event.vkeycode == VK_RIGHT) {
			if (activePanel->activeItem->type == Explorer::Item::Type_Directory)
				ActionOpenDirectory(this);
			
		} else if (event.vkeycode == VK_LEFT) {
			
			Panel* closingPanel = activePanel;
			activePanel = closingPanel->parent;
			delete closingPanel;
			
			if (!activePanel) {
				shouldClose = true;
				return;
			}
			
			activeItemAnimationValue = 0.0f;
			
		} else if (event == settings.keybinds.copy) {
			ActionCopyOrCut(this, false);
	
		} else if (event == settings.keybinds.cut) {
			ActionCopyOrCut(this, true);
			
		} else if (event == settings.keybinds.paste) {
			ActionPaste(this);
			
		} else if (event == settings.keybinds.deleteNextChar) {
			ActionDelete(this);
			
		} else if (event == settings.keybinds.explorerShellExecute) {
			ActionShellExecute(this);
			
		} else if (event == settings.keybinds.explorerOpenInWindowsExplorer) {
			ActionRevealInExplorer(this);
		
		} else if (event == settings.keybinds.explorerNewFile) {
			ActionNewItem(this, Item::Type_File);
		
		} else if (event == settings.keybinds.explorerNewFolder) {
			ActionNewItem(this, Item::Type_Directory);
		
		} else if (event == settings.keybinds.explorerRename) {
			ActionRenameItem(this);
		
		} else if (event.vkeycode == VK_RETURN) {
			ActionOpenItem(this, OpenBehaviorFromModifiers(event));
			return;
		
		} else if (event.vkeycode == VK_ESCAPE) {
			shouldClose = true;
			return;
		}
	}
}

void Explorer::OnChar(const char* utf8, u64 len) {
	if (newItemDialog)
		newItemDialog->textbox.OnChar(utf8, len);
}

Explorer::~Explorer() noexcept {}