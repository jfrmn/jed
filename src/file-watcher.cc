#include "file-watcher.hh"
#include "util/logging.hh"
#include "util/string-util.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr u64 BUFFER_SIZE = 1024;
static_assert(sizeof(FileWatcher::buffer) == BUFFER_SIZE);

static_assert(FileWatcher::Action_Added == FILE_ACTION_ADDED);
static_assert(FileWatcher::Action_Removed == FILE_ACTION_REMOVED);
static_assert(FileWatcher::Action_Modified == FILE_ACTION_MODIFIED);
static_assert(FileWatcher::Action_RenamedOldName == FILE_ACTION_RENAMED_OLD_NAME);
static_assert(FileWatcher::Action_RenamedNewName == FILE_ACTION_RENAMED_NEW_NAME);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static VOID CompletionRoutine(DWORD errorCode, DWORD numberOfBytesTransfered, OVERLAPPED* overlapped);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct WatchJob {
	void* hDirectory = nullptr;
	u8* buffer = nullptr;
	
	void* userdata = nullptr;
	FileWatcher::OnChangeHandler OnChange = nullptr;
	
	OVERLAPPED overlapped = {};
	
	u32 notifyFilter = 0u;
	
	bool StartReadingDirectoryChanges() {
		const bool res = ReadDirectoryChangesW(
			/* hDirectory */          hDirectory,
			/* lpBuffer */            buffer,
			/* nBufferLength */       BUFFER_SIZE,
			/* bWatchSubtree */       true,
			/* dwNotifyFilter */      notifyFilter,
			/* lpBytesReturned */     nullptr,
			/* lpOverlapped */        &overlapped,
			/* lpCompletionRoutine */ CompletionRoutine);
			
		if (!res) {
			LogError("ReadDirectoryChangesW() failed. Last Error: %", FLastErr(GetLastError()));
			return false;
		}
		
		return true;
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void StartReadingDirectoryChanges(ULONG_PTR parameter) {
	auto self = reinterpret_cast<WatchJob*>(parameter);
	self->StartReadingDirectoryChanges();
}

static VOID CompletionRoutine(DWORD errorCode, DWORD numberOfBytesTransfered, OVERLAPPED* overlapped) {
	
	if (errorCode == ERROR_OPERATION_ABORTED) return;
	if (errorCode != 0u) {
		LogError("ReadDirectoryChangesW() completed with error-code: %", errorCode);
		return;
	}
	
	auto self = static_cast<WatchJob*>(overlapped->hEvent);
	
	u64 offset = 0u;
	auto fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(self->buffer);
	while (true) {
		const u64 wFileNameLength = fileNotifyInfo->FileNameLength / sizeof(wchar);
		const std::wstring_view wFileName {fileNotifyInfo->FileName, wFileNameLength};
		
		char utf8Buf[_MAX_PATH] {0}; u64 utf8Len = 0u;
		ToUtf8(wFileName, utf8Buf, &utf8Len);
		const std::string_view fileName {utf8Buf, utf8Len};
		
		self->OnChange(self->userdata, static_cast<FileWatcher::Action>(fileNotifyInfo->Action), fileName);
		
		if (fileNotifyInfo->NextEntryOffset == 0u)
			break;
			
		offset += fileNotifyInfo->NextEntryOffset;
		fileNotifyInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(self->buffer + offset);
	}
	
	memset(self->buffer, 0, BUFFER_SIZE);
	self->StartReadingDirectoryChanges();
}

static DWORD WINAPI ThreadProc(LPVOID userdata) {
	auto self = static_cast<FileWatcher*>(userdata);
	
	while (true) {
		const u32 result = WaitForSingleObjectEx(self->hEventExitThread, INFINITE, TRUE);
		if      (result == WAIT_IO_COMPLETION) continue;
		else if (result == WAIT_OBJECT_0) break;
		else {
			LogError("WaitForSingleObject() in ThreadProc returned: %", FWaitRes(result));
			return 1l;
		}
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
		LogError("CreateEventA() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}
		
	LogInfo("creating file-watcher thread");
	
	hThread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
	if (hThread == NULL) {
		LogError("failed to create file-watcher thread. Last Error: %", FLastErr(GetLastError()));
		return false;
	}

	return true;
}

void FileWatcher::Shutdown() {

	for (WatchJob* job : watchJobs)
		CancelIo(job->hDirectory);
		
	SetEvent(hEventExitThread);
	
	const u32 waitRes = WaitForSingleObject(hThread, 5000);
	if (waitRes != WAIT_OBJECT_0)
		LogWarning("file-watcher thread did not return in time. Result: %", FWaitRes(waitRes));
	
	for (WatchJob* job : watchJobs) {
		CloseHandle(job->hDirectory);
		delete job;
	}
	
	CloseHandle(hEventExitThread);
	CloseHandle(hThread);
}

bool FileWatcher::WatchDirectory(std::string_view path, u32 notifyFilter, FileWatcher::OnChangeHandler OnChange, void* userdata /*= nullptr*/) {
	
	HANDLE hDir = CreateFile(
		/*lpFileName*/            path.data(),
		/*dwDesiredAccess*/       FILE_LIST_DIRECTORY,
		/*dwShareMode*/           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		/*lpSecurityAttributes*/  NULL,
		/*dwCreationDisposition*/ OPEN_EXISTING,
		/*dwFlagsAndAttributes*/  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		/*hTemplateFile*/         NULL);
	
	if (hDir == NULL) {
		LogError("CreateFile() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}
	
	WatchJob* job = new WatchJob {
		.hDirectory = hDir,
		.buffer = buffer,
		.userdata = userdata,
		.OnChange = OnChange,
		.overlapped = OVERLAPPED {},
		.notifyFilter = notifyFilter};
	job->overlapped.hEvent = job;
	const DWORD ok = QueueUserAPC(StartReadingDirectoryChanges, hThread, reinterpret_cast<ULONG_PTR>(job));
	if (ok == 0u) LogError("QueueUserAPC() failed. Last Error: %", FLastErr(GetLastError()));
	
	watchJobs.push_back(job);
	return true;
}
