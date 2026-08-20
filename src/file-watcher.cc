#include "file-watcher.hh"
#include "logging.hh"
#include "util/string-util.hh"
#include "main-window.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr u64 BUFFER_SIZE = 1024;
static_assert(sizeof(FileWatcher::buffer) == BUFFER_SIZE);

static_assert(FileChangeRecord::Action_Added == FILE_ACTION_ADDED);
static_assert(FileChangeRecord::Action_Removed == FILE_ACTION_REMOVED);
static_assert(FileChangeRecord::Action_Modified == FILE_ACTION_MODIFIED);
static_assert(FileChangeRecord::Action_RenamedOld == FILE_ACTION_RENAMED_OLD_NAME);
static_assert(FileChangeRecord::Action_RenamedNew == FILE_ACTION_RENAMED_NEW_NAME);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static VOID CompletionRoutine(DWORD errorCode, DWORD numberOfBytesTransfered, OVERLAPPED* overlapped);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct WatchedDirectory {
	std::string path = {};
	
	HANDLE hDirectory = nullptr;
	OVERLAPPED overlapped = {};	
	
	u64 references = 0u;
	std::mutex mtx = {};
		
	bool StartReadingDirectoryChanges() {
		const bool res = ReadDirectoryChangesW(
			/* hDirectory */          hDirectory,
			/* lpBuffer */            fileWatcher.buffer,
			/* nBufferLength */       BUFFER_SIZE,
			/* bWatchSubtree */       false,
			/* dwNotifyFilter */      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
			/* lpBytesReturned */     nullptr,
			/* lpOverlapped */        &overlapped,
			/* lpCompletionRoutine */ CompletionRoutine);
			
		if (!res) {
			LogWarning("ReadDirectoryChangesW() failed. Last Error: %s", StrLastErr(GetLastError()));
			return false;
		}
		
		return true;
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void StartReadingDirectoryChanges(ULONG_PTR parameter) {
	auto self = reinterpret_cast<WatchedDirectory*>(parameter);
	self->StartReadingDirectoryChanges();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void CloseDirectory(ULONG_PTR parameter) {
	auto self = reinterpret_cast<WatchedDirectory*>(parameter);
	CancelIo(self->hDirectory);
	CloseHandle(self->hDirectory);
	delete self;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static VOID CompletionRoutine(DWORD errorCode, DWORD numberOfBytesTransfered, OVERLAPPED* overlapped) {
	
	if (errorCode == ERROR_OPERATION_ABORTED) return;
	if (errorCode != 0u) {
		LogError("ReadDirectoryChangesW() completed with error-code: %u", errorCode);
		return;
	}
	
	auto self = static_cast<WatchedDirectory*>(overlapped->hEvent);
	const std::scoped_lock lock {self->mtx};
	
	//
	// figure out the required size
	//
	u64 recordCount = 0u;
	u64 requiredSize = sizeof(FileChangedEvent) - sizeof(FileChangeRecord);
	{
		auto fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(fileWatcher.buffer);
		u64 offset = 0u;
		while (true) {			
			recordCount += 1u;
			requiredSize += sizeof(FileChangeRecord);
			
			u64 utf8FilenameLen = 0u;
			ToUtf8(fileNotifyInfo->FileName, {}, &utf8FilenameLen);
			requiredSize += utf8FilenameLen;
			
			if (fileNotifyInfo->NextEntryOffset == 0u) break;
			offset += fileNotifyInfo->NextEntryOffset;
			fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(fileWatcher.buffer + offset);
		}
		
		requiredSize += self->path.size();
	}
	
	u8* buffer = static_cast<u8*>(malloc(requiredSize));
	
	auto fileChangedEvent = reinterpret_cast<FileChangedEvent*>(buffer);
	fileChangedEvent->recordCount = recordCount;
	
	char* fileNameData = reinterpret_cast<char*>(&fileChangedEvent->records[recordCount]);
	char* fileNameDataEnd = reinterpret_cast<char*>(buffer + requiredSize);
	
	//
	// copy the directory path
	//
	{
		memcpy(fileNameData, self->path.data(), self->path.size());
		fileChangedEvent->directory = fileNameData;
		fileChangedEvent->directoryLength = self->path.size();
		fileNameData += self->path.size();
	}
	
	//
	// fill the records
	//	
	{
		auto fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(fileWatcher.buffer);
		u64 offset = 0u;
		FileChangeRecord* record = fileChangedEvent->records;
		while (true) {
			record->action = static_cast<FileChangeRecord::Action>(fileNotifyInfo->Action);
			
			ToUtf8(fileNotifyInfo->FileName, {fileNameData, fileNameDataEnd}, &record->filenameLength);
			record->filename = fileNameData;
			
			fileNameData += record->filenameLength;
			record++;
			
			if (fileNotifyInfo->NextEntryOffset == 0u) break;
			
			offset += fileNotifyInfo->NextEntryOffset;
			fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(fileWatcher.buffer + offset);
		}
	}
	ASSERT(fileNameData == fileNameDataEnd);
	
	//
	// send event
	//
	mainWindow.PostFileChangedEvent(fileChangedEvent);
	
	//
	// reset and restart reading
	//
	memset(fileWatcher.buffer, 0, BUFFER_SIZE);
	self->StartReadingDirectoryChanges();
}

DWORD WINAPI ThreadProcWatcher(LPVOID userdata) {
	auto self = static_cast<FileWatcher*>(userdata);
	
	while (true) {
		const u32 result = WaitForSingleObjectEx(self->hEventExitThread, INFINITE, TRUE);
		if      (result == WAIT_IO_COMPLETION) continue;
		else if (result == WAIT_OBJECT_0) break;
		else {
			LogError("WaitForSingleObject() in ThreadProcWatcher returned: %s", StrWaitRes(result));
			return 1l;
		}
	}
	
	for (WatchedDirectory* watchedDir : self->watchedDirectories) {
		CancelIo(watchedDir->hDirectory);
		CloseHandle(watchedDir->hDirectory);
		delete watchedDir;
	}
	return 0l;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool FileWatcher::Init() {

	hEventExitThread = CreateEventA(
		/* lpEventAttributes */ nullptr,
		/* bManualReset */      TRUE,
		/* bInitialState */     FALSE,
		/* lpName */            nullptr);
		
	if (!hEventExitThread) {
		LogError("CreateEventA() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}
		
	LogInfo("creating file-watcher thread");
	
	hThread = CreateThread(nullptr, 0, ThreadProcWatcher, this, 0, nullptr);
	if (hThread == NULL) {
		LogError("failed to create file-watcher thread. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}
	
	return true;
}

void FileWatcher::Shutdown() {

	SetEvent(hEventExitThread);
	
	const u32 waitRes = WaitForSingleObject(hThread, 5000);
	if (waitRes != WAIT_OBJECT_0)
		LogWarning("file-watcher thread did not return in time. Result: %s", StrWaitRes(waitRes));
	
	CloseHandle(hEventExitThread);
	CloseHandle(hThread);
}

bool FileWatcher::SubscribeDirectoryOfFile(std::string_view filepath) {
	const u64 posDelimiter = filepath.find_last_of("/\\");
	
	// @TODO could be improved
	// if no delimiter is found than the entire path is the filename and the directory is the cwd
	ASSERT(posDelimiter != std::string::npos)
	ASSERT(posDelimiter + 1u <= filepath.length());
	
	// need to copy the directory to a buffer because the CreateFile() function takes a null-terminated string
	char directoryBuffer[_MAX_PATH] {0};
	memcpy(directoryBuffer, filepath.data(), posDelimiter);
	directoryBuffer[posDelimiter] = '\0';
	
	const std::string_view directory {directoryBuffer, posDelimiter};
	
	for (WatchedDirectory* watchedDirectory : watchedDirectories) {
		if (watchedDirectory->path == directory) {
			watchedDirectory->references++;
			return true;
		}
	}
	
	// create new watchedDirectory	
	HANDLE hDir = CreateFile(
		/*lpFileName*/            directoryBuffer,
		/*dwDesiredAccess*/       FILE_LIST_DIRECTORY,
		/*dwShareMode*/           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		/*lpSecurityAttributes*/  NULL,
		/*dwCreationDisposition*/ OPEN_EXISTING,
		/*dwFlagsAndAttributes*/  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		/*hTemplateFile*/         NULL);
	
	if (hDir == INVALID_HANDLE_VALUE) {
		LogError("CreateFile() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}
	
	auto newWatchedDirectory = new WatchedDirectory {
		.path = std::string {directory},
		.hDirectory = hDir,
		.overlapped = OVERLAPPED {},
		.references = 1u,
		.mtx = {}};
	newWatchedDirectory->overlapped.hEvent = newWatchedDirectory;
	
	watchedDirectories.push_back(newWatchedDirectory);	
	
	const DWORD ok = QueueUserAPC(StartReadingDirectoryChanges, hThread, reinterpret_cast<ULONG_PTR>(newWatchedDirectory));
	if (ok == 0u) {
		LogError("QueueUserAPC() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}
	
	return true;
}

bool FileWatcher::UnsubscribeDirectoryOfFile(std::string_view filepath) {
	
	const u64 posDelimiter = filepath.find_last_of("/\\");
	
	// @TODO see above -- could be improved
	// if no delimiter is found than the entire path is the filename and the directory is the cwd
	ASSERT(posDelimiter != std::string::npos)
	ASSERT(posDelimiter + 1u < filepath.length());
	
	const std::string_view directory = filepath.substr(0u, posDelimiter);
	const std::string_view filename = filepath.substr(posDelimiter + 1u);
	
	for (u64 i = 0u; i < watchedDirectories.size(); i++) {
		WatchedDirectory* watchedDirectory = watchedDirectories[i];
		
		if (watchedDirectory->path == directory) {
			if (--watchedDirectory->references > 0u) return true;
			
			std::swap(watchedDirectories[i], watchedDirectories.back());
			watchedDirectories.pop_back();
			
			const DWORD ok = QueueUserAPC(CloseDirectory, hThread, reinterpret_cast<ULONG_PTR>(watchedDirectory));
			if (ok == 0u) {
				LogError("QueueUserAPC() failed. Last Error: %s", StrLastErr(GetLastError()));
				return false;
			}
			
			return true;
		}
	}
	
	LogError("directory '%.*s' is not watched", SIZE_AND_DATA(filepath));
	return false;
}

