#include "file-search-bar.hh"
#include "basic.hh"
#include "main-window.hh"
#include "globals.hh"
#include "events.hh"

#include "util/file-util.hh"
#include "util/format.hh"
#include "util/logging.hh"

#include <algorithm>


FileSearchBar* FileSearchBar::Make() {
	auto self = new FileSearchBar();
	if (!self->Init("find file...", false)) {
		delete self;
		return nullptr;
	}
	return self;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static std::string_view GetFilename(const FileSearchBar::Item& item) {
	return std::string_view {item.fullPath.data() + item.fullPath.size() - item.filenameLength, item.filenameLength}; 
}

static bool SearchDirectory(FileSearchBar* self, std::string_view text, std::string searchPath) {

	DirectoryIterator dirIter {std::move(searchPath)};
	while (dirIter.Next()) {

		const std::string_view filename {dirIter.filename};

		// search subdirectory
		if (dirIter.IsDirectory()) {
		
			const std::string subDir = FormatString("%\\%\\", dirIter.GetSearchPath(), dirIter.filename);
		
			SearchDirectory(self, text, subDir);
			continue;
		}
		
		FuzzyMatchResult matchResult {};
		if (!FuzzyMatch(text, filename, &matchResult))
			continue;
		
		if (matchResult.matchedCount < 3)
			continue;
				
		const std::string_view path = dirIter.GetSearchPath();
		
		FileSearchBar::Item& item = self->items.emplace_back();
		item.fullPath.reserve(path.size() + 1 + filename.size()); // +1 for the \ before the filename
		item.fullPath.append(path);
		item.fullPath.push_back('\\');
		item.fullPath.append(filename);
		
		item.filenameLength = filename.size();
		item.fuzzyMatchResult = matchResult;
	}

	if (dirIter.Failed()) {
		LogError("failed to search directory: '%'. Last Error: %", dirIter.searchPattern, FLastErr(dirIter.lastError));
		return false;
	}

	return true;
}

static bool CompareItems(const FileSearchBar::Item& litem, const FileSearchBar::Item& ritem) {
	if (const int cmp = CompareFuzzyMatchResults(litem.fuzzyMatchResult, ritem.fuzzyMatchResult); cmp != 0)
		return cmp > 0;
					
	if (litem.filenameLength < ritem.filenameLength)
		return true;	
	if (litem.filenameLength > ritem.filenameLength)
		return false;

	const std::string_view lfilename = GetFilename(litem);
	const std::string_view rfilename = GetFilename(ritem);
	if (const int cmp = lfilename.compare(rfilename); cmp != 0)
		return cmp < 0;
						
	if (litem.fullPath.size() < ritem.fullPath.size())
		return true;
	if (litem.fullPath.size() > ritem.fullPath.size())
		return false;
		 
	return litem.fullPath < ritem.fullPath;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
u64 FileSearchBar::FilterItems(std::string_view searchText) {
	
	items.clear();
	if (searchText.size() < 3u) return 0u;
	
	SearchDirectory(this, searchText, {".", 1});
		
	std::sort(items.begin(), items.end(), CompareItems);
	return items.size();
}

void FileSearchBar::AcceptItem(u64 i, const KeyEvent* event) {
	ASSERT(i < items.size());
	const Item& item = items[i];
	
	MainWindow::OpenBehavior openBehav = event
		? OpenBehaviorFromModifiers(*event)
		: MainWindow::OpenBehavior_Default;
	
	const bool ok = mainWindow.OpenEditor(item.fullPath, openBehav);
	if (!ok) LogError("OpenEditor() failed");
	
	shouldClose = true;
}

void FileSearchBar::GetItemInfo(u64 i, /*out*/ ItemInfo* itemInfo) {
	ASSERT(i < items.size());
	const Item& item = items[i];
	
	*itemInfo = SearchBar::ItemInfo {
		.text = GetFilename(item),
		.subText = item.fullPath,
		.matchedPosition = item.fuzzyMatchResult.position,
		.matchedLength = item.fuzzyMatchResult.length};
}

