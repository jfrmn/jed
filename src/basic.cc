#include "basic.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>

#include <stdio.h>

static void PrintAssertMessage(const char* expression, const char* file, int line, const char* function, bool soft) {

	printf("\x1b[%dmASSERT\x1b[0m: \"%s\" @ %s:%d(%s)\n", soft ? 43 : 41, expression, file, line, function);
	
#ifdef _DEBUG
	HANDLE hProcess = GetCurrentProcess();
	if (!SymInitialize(hProcess, NULL, TRUE))
		puts("SymInitialize failed\n");

	void* frameAddresses[32] {0};
	const u32 frameCount = CaptureStackBackTrace(0, STATIC_ARRAY_SIZE(frameAddresses), frameAddresses, NULL);
	
	alignas(SYMBOL_INFO) u8 symbolBuffer[sizeof(SYMBOL_INFO) + 256] {0};
	auto symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO),
	symbol->MaxNameLen = 256;
	
	for (u32 i = 0u; i < frameCount; i++) {
		SymFromAddr(hProcess, reinterpret_cast<DWORD64>(frameAddresses[i]), 0, symbol);
		printf(" > 0x%0llX %.*s()\n", symbol->Address, static_cast<int>(symbol->NameLen), symbol->Name);
	}
#endif
	
	fflush(stdout);
}

void _TriggerHardAssert(const char* expression, const char* file, int line, const char* function) {
	PrintAssertMessage(expression, file, line, function, false);
	DebugBreak();
}

void _TriggerSoftAssert(const char* expression, const char* file, int line, const char* function) {
	PrintAssertMessage(expression, file, line, function, false);
	
	char buffer[256] {0};
	sprintf_s(buffer, "expr: %s\nfile: %s\nline: %d\nfunction: %s\n\nPress OK to continue or Cancel to terminate.", expression, file, line, function);
	
	const DWORD result = MessageBoxA(NULL, buffer, "Soft Assert", MB_OKCANCEL | MB_ICONWARNING);
	if (result == IDCANCEL) ExitProcess(0xff);
		
}

