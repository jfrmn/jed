#include "file-search-bar.hh"
#include "basic.hh"
#include "main-window.hh"
#include "globals.hh"

#include "util/file-util.hh"
#include "logging.hh"

#include <algorithm>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//static constexpr u64 MAX_INTERMEDIATE_ITEMS = STATIC_ARRAY_SIZE(FileSearchBar::ThreadData::intermediateItems);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FileSearchBar::ThreadData::RemoveReference() {
	if (--references <= 0) delete this;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
FileSearchBar* FileSearchBar::Make() {
	auto self = new FileSearchBar();
	if (!self->Init("find file...", false)) {
		delete self;
		return nullptr;
	}
	return self;
}

FileSearchBar::~FileSearchBar() noexcept {
	if (threadData) {
		ASSERT(hThread);	
		
		threadData->isCancelled = true;
		threadData->RemoveReference();
		threadData = nullptr;
		
		CloseHandle(hThread);
		hThread = NULL;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static std::string_view GetFilename(const FileSearchBar::Item& item) {
	return std::string_view {item.fullPath.data() + item.fullPath.size() - item.filenameLength, item.filenameLength}; 
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



//static void FlushIntermediateItems(FileSearchBar::ThreadData* threadData) {
//	const std::scoped_lock lock {threadData->searchBar->mtx};
//	
//	auto& items = threadData->searchBar->items;
//	items.reserve(items.size() + threadData->intermediateItemsCount);
//	
//	for (u64 i = 0u; i < threadData->intermediateItemsCount; i++) {
//		auto& intermediateItem = threadData->intermediateItems[i];
//		
//		items.push_back(intermediateItem);
//		intermediateItem.fullPath.clear();
//		intermediateItem.filenameLength = 0u;
//		intermediateItem.fuzzyMatchResult = {};
//	}
//	
//	threadData->intermediateItemsCount = 0;
//	std::sort(items.begin(), items.end(), CompareItems);
//	
//	mainWindow.PostFunctionCall(0, [] (void*) {
//		auto fileSearchBar = dynamic_cast<FileSearchBar*>(mainWindow.searchBar);
//		if (!fileSearchBar) return;
//		if (!fileSearchBar->threadData) return;
//		
//		const std::scoped_lock lock {fileSearchBar->mtx};
//		fileSearchBar->SetItemCount(fileSearchBar->items.size());
//	});
//}

static void SearchDirectory(FileSearchBar::ThreadData* td, DirectoryIterator& iterator) {
		
	while (iterator.Next()) {
		
		if (td->isCancelled)
			return;
		
		if (iterator.IsDirectory()) {
			
			DirectoryIterator subDirIterator;
			strcpy_s(subDirIterator.searchPath, iterator.searchPath);
			char* searchPathTail = strrchr(subDirIterator.searchPath, '*');
			ASSERT(searchPathTail);
			
			memcpy(searchPathTail, iterator.filename.data(), iterator.filename.size());
			searchPathTail += iterator.filename.size();
			*searchPathTail++ = '\\';
			*searchPathTail++ = '*';
			*searchPathTail++ = '\0'; // the debug version of strcpy_s fills the buffer with 0xfe before copying
			
			SearchDirectory(td, subDirIterator);
			continue;
		}
		
		FuzzyMatchResult matchResult {};
		if (!FuzzyMatch(td->searchTerm, iterator.filename, &matchResult))
			continue;
		
		if (matchResult.matchedCount < 3)
			continue;
				
		const std::string_view path = iterator.GetSearchPath();
		
		FileSearchBar::Item item {};
		item.fullPath.reserve(path.size() + 1 + iterator.filename.size()); // +1 for the \ before the filename
		item.fullPath.append(path);
		item.fullPath.push_back('\\');
		item.fullPath.append(iterator.filename);
		
		item.filenameLength = iterator.filename.size();
		item.fuzzyMatchResult = matchResult;	
		
		const std::scoped_lock lock {td->mtxResults};
		auto itWhere = std::find_if(td->results.begin(), td->results.end(), [&item] (const FileSearchBar::Item& other) {
			return CompareItems(item, other);
		});
		
		td->results.insert(itWhere, std::move(item));
	}
		
	if (iterator.Failed())
		LogError("failed to search directory: '%s'. Last Error: %s", iterator.GetSearchPath().data(), StrLastErr(iterator.lastError));
}

DWORD WINAPI ThreadProcSearchDirectory(LPVOID param) {
	auto threadData = static_cast<FileSearchBar::ThreadData*>(param);
	
	DirectoryIterator iter {"."};
	SearchDirectory(threadData, iter);
	
//	FlushIntermediateItems(threadData);
	
	mainWindow.SendUpdate();
	
	threadData->isComplete = true;
	threadData->RemoveReference();
	return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FileSearchBar::FilterItems(std::string_view searchText) {

	if (threadData) {
		ASSERT(hThread);	
		
		threadData->isCancelled = true;
		threadData->RemoveReference();
		threadData = nullptr;
		
		CloseHandle(hThread);
		hThread = NULL;
	}
	
	SetItemCount(0u);
	
	if (searchText.size() < 3u) return;
		
	threadData = new ThreadData();
	threadData->searchBar = this;
	threadData->searchTerm = searchText;
	threadData->references = 2;
	
	hThread = CreateThread(NULL, 0, ThreadProcSearchDirectory, threadData, 0, nullptr);
	if (hThread == NULL) {
		LogError("CreateThread() failed. Last Error: %s", StrLastErr(GetLastError()));
		
		delete threadData;
		threadData = nullptr;
	}
}

void FileSearchBar::OnUpdateItems(u64 firstVisible, u64 lastVisible) {
	if (!threadData) return;
	
	const std::scoped_lock lock {threadData->mtxResults};
	ASSERT(firstVisible <= lastVisible)
	ASSERT(lastVisible <= threadData->results.size())
	
	if (!threadData->isComplete) {
		SetItemCount(threadData->results.size());
		needsUpdate = true;
	}
	
	for (u64 i = firstVisible; i < lastVisible; i++) {
		const Item& item = threadData->results[i];
				
		UpdateItem(i, UpdateItemParams {
			.text = GetFilename(item),
			.subText = item.fullPath,
			.matchedPosition = item.fuzzyMatchResult.position,
			.matchedLength = item.fuzzyMatchResult.length});
	}
}

void FileSearchBar::AcceptItem(u64 i, const KeyEvent* event) {
	
	const std::scoped_lock lock {threadData->mtxResults};
	ASSERT(threadData);
	ASSERT(i < threadData->results.size());
	
	const Item& item = threadData->results[i];
	
	MainWindow::OpenBehavior openBehav = event
		? OpenBehaviorFromModifiers(*event)
		: MainWindow::OpenBehavior_Default;
	
	const bool ok = mainWindow.OpenEditor(item.fullPath, openBehav);
	if (!ok) LogError("OpenEditor() failed");
	
	shouldClose = true;
}
