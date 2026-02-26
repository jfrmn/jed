#include "process.hh"
#include "util/logging.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define THREAD_EXIT_GRACEFUL   0
#define THREAD_EXIT_WITHERROR -1
#define READ_BUFFER_SIZE 512

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static DWORD WINAPI ReadThreadProc(LPVOID userdata);

bool Process::StartDetached(StartInfo startInfo) {
	
	STARTUPINFOA startupInfo {sizeof(STARTUPINFOA)};
	PROCESS_INFORMATION processInfo {};
	
	if (!CreateProcessA(
			(startInfo.application.empty() ? NULL : startInfo.application.data()),
			startInfo.commandLine.data(),
			NULL,
			NULL,
			FALSE,
			startInfo.flags,
			NULL,
			NULL,
			&startupInfo,
			&processInfo)) {
		LogError("CreateProcess() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}

	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Process::Start(StartInfo startInfo) {

	HANDLE hPipeStdinRead  = NULL, hPipeStdinWrite  = NULL;
	HANDLE hPipeStdoutRead = NULL, hPipeStdoutWrite = NULL;
	HANDLE hPipeStderrRead = NULL, hPipeStderrWrite = NULL;

	//
	// create pipes for stdin/stdout/stderr
	//
	{

		SECURITY_ATTRIBUTES securityAttributes {sizeof(SECURITY_ATTRIBUTES)};
		securityAttributes.bInheritHandle = TRUE;
		securityAttributes.lpSecurityDescriptor = NULL;

		if (!CreatePipe(&hPipeStdinRead, &hPipeStdinWrite, &securityAttributes, 0)) {
			LogError("CreatePipe() failed for stdin. Last Error: %", FLastErr(GetLastError()));
			return false;
		}

		if (!CreatePipe(&hPipeStdoutRead, &hPipeStdoutWrite, &securityAttributes, 0)) {
			LogError("CreatePipe() failed for stdout. Last Error: %", FLastErr(GetLastError()));
			return false;
		}

		if (!CreatePipe(&hPipeStderrRead, &hPipeStderrWrite, &securityAttributes, 0)) {
			LogError("CreatePipe() failed for stderr. Last Error: %", FLastErr(GetLastError()));
			return false;
		}

		SetHandleInformation(hPipeStdinWrite, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(hPipeStdoutRead, HANDLE_FLAG_INHERIT, 0);
		SetHandleInformation(hPipeStderrRead, HANDLE_FLAG_INHERIT, 0);

		hPipeStdin  = hPipeStdinWrite;
	}

	//
	// create child-process
	//
	{
		STARTUPINFOA startupInfo {sizeof(STARTUPINFOA)};
		startupInfo.hStdOutput = hPipeStdoutWrite;
		startupInfo.hStdError = hPipeStderrWrite;
		startupInfo.hStdInput = hPipeStdinRead;
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;

		PROCESS_INFORMATION processInfo {};
		
		if (!CreateProcessA(
				(startInfo.application.empty() ? NULL : startInfo.application.data()),
				startInfo.commandLine.data(),
				NULL,
				NULL,
				TRUE, // inherint handles
				startInfo.flags,
				NULL,
				NULL,
				&startupInfo,
				&processInfo)) {
			LogError("CreateProcess() failed. Last Error: %", FLastErr(GetLastError()));
			return false;
		}

		hProcess = processInfo.hProcess;
		hProcessMainThread = processInfo.hThread;
	}

	//
	// close now unused handles
	//
	{
		CloseHandle(hPipeStdoutWrite);
		CloseHandle(hPipeStderrWrite);
		CloseHandle(hPipeStdinRead);
	}

	//
	// launch threads to read to stdout
	//
	{
		DWORD threadId = 0;

		threadDataStdout = ThreadData {hPipeStdoutRead, this};
		hThreadReadStdout = CreateThread(NULL, 0, ReadThreadProc, &threadDataStdout, 0, &threadId);

		if (hThreadReadStdout == NULL) {
			LogError("CreateThread() failed. Last Error: %", FLastErr(GetLastError()));
			TerminateProcess(hProcess, -1);
			return false;
		}

		threadDataStderr = ThreadData {hPipeStderrRead, this};
		hThreadReadStderr = CreateThread(NULL, 0, ReadThreadProc, &threadDataStderr, 0, &threadId);

		if (hThreadReadStderr == NULL) {
			LogError("CreateThread() failed. Last Error: %", FLastErr(GetLastError()));
			TerminateProcess(hProcess, -1);
			return false;
		}
	}

	return true;
}

u32 Process::GetExitCode() const {
	DWORD exitCode = 0;
	GetExitCodeProcess(hProcess, &exitCode);
	return exitCode;
}

bool Process::IsRunning() const {
	return GetExitCode() == STILL_ACTIVE;
}

bool Process::Terminate(u32 exitCode /*= U32_MAX*/) {
	return TerminateProcess(hProcess, exitCode);
}

bool Process::WriteToStdin(std::string_view data) {
	
	DWORD numBytesWritten = 0;
	const BOOL res = WriteFile(hPipeStdin, data.data(), static_cast<u32>(data.length()), &numBytesWritten, nullptr);

	ASSERT(numBytesWritten == data.length());
	if (!res) {
		LogError("WriteFile() to stdin of pid % failed. LastError: %", GetProcessId(hProcess), FLastErr(GetLastError()));
		return false;
	}

	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Process::Process(Process &&other) noexcept {	
	std::swap(this->hProcess, other.hProcess);
	std::swap(this->hProcessMainThread, other.hProcessMainThread);
	std::swap(this->hThreadReadStdout, other.hThreadReadStdout);
	std::swap(this->hThreadReadStderr, other.hThreadReadStderr);
	std::swap(this->hPipeStdin, other.hPipeStdin);
	std::swap(this->threadDataStdout, other.threadDataStdout);
	std::swap(this->threadDataStderr, other.threadDataStderr);
	std::swap(this->observer, other.observer);
}

Process::~Process() noexcept {
	TerminateProcess(hProcess, U32_MAX);
	
	const HANDLE handles[] {hProcess, hProcessMainThread, hThreadReadStdout, hThreadReadStderr};
	WaitForMultipleObjects(STATIC_ARRAY_SIZE(handles), handles, true, INFINITE); 
	
	CloseHandle(hProcess);	
	CloseHandle(hProcessMainThread);	
	CloseHandle(hThreadReadStdout);	
	CloseHandle(hThreadReadStderr);	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static DWORD WINAPI ReadThreadProc(LPVOID userdata) {

	auto threadData = static_cast<Process::ThreadData*>(userdata);
	auto observer = threadData->process->observer;

	const bool isStdout = (threadData == &threadData->process->threadDataStdout);
	
	if (isStdout)
		observer->OnStarted();

	usize totalSize = 0u;
	std::string buffer(READ_BUFFER_SIZE, '\0');

	while (true) {

		DWORD numBytesRead = 0;
		const BOOL success = ReadFile(
			threadData->hPipe,
			buffer.data() + totalSize, 
			static_cast<DWORD>(buffer.size() - totalSize),
			&numBytesRead,
			NULL);

		totalSize += numBytesRead;
		
		// successfull read
		if (success) {
				
			const std::string_view bufferView {buffer.data(), totalSize};
			
			if (isStdout)
				observer->OnStdout(bufferView);
			else
				observer->OnStderr(bufferView);
				
			memset(buffer.data(), 0, totalSize);
			totalSize = 0u;
		
		// bad read
		} else {

			const DWORD lastError = GetLastError();

			// buffer not big enough
			if (lastError == ERROR_MORE_DATA) {
				
				buffer.resize(static_cast<usize>(buffer.size() * 1.5f));
				continue;
			
			// process exited
			} else if (lastError == ERROR_BROKEN_PIPE) {
				
				if (isStdout) {
					int processExitCode = STILL_ACTIVE; 
					while (processExitCode == STILL_ACTIVE) {
						processExitCode = threadData->process->GetExitCode();
						Sleep(100);
					}

					observer->OnExited(processExitCode);
				}

				return THREAD_EXIT_GRACEFUL;
			
			// some actual error occured
			} else {
				LogWarning("ReadFile() on % failed. Last Error: %", (isStdout ? "stdout" : "stderr"), FLastErr(lastError));
				return THREAD_EXIT_WITHERROR;
			}
		}
	}

	ASSERT_UNREACHABLE;
	return THREAD_EXIT_WITHERROR;
}