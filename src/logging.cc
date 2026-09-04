#include "logging.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <comdef.h>
#include <dbghelp.h>

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 1
#include <toml++/toml.hpp>

#include <stdio.h>
#include <io.h>
#include <fcntl.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static const char* logLevelPrefix[] = {
	nullptr /* OFF */,
	"\x1b[35mDEV:    \x1b[0m",
    "\x1b[41mFATAL:  \x1b[0m",
	"\x1b[31mERROR:  \x1b[0m",
	"\x1b[33mWARN:   \x1b[0m",
	"\x1b[32mINFO:   \x1b[0m",
	"\x1b[90mTRACE:  \x1b[0m"
};

void Logger::Log(LogLevel msgLevel, const char* fmt, va_list args) {
	
	if (level >= msgLevel) {
		
		if (mtx)
			mtx->lock();

		fputs(logLevelPrefix[msgLevel], out);
		
		if (!prefix.empty()) {
			fputs("\x1b[36m", out);
			fwrite(prefix.data(), 1, prefix.size(), out);
			fputs("\x1b[0m ", out);
		}
		
		vfprintf(out, fmt, args);
		fputc('\n', out);
		fflush(out);

		if (mtx)
			mtx->unlock();
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static std::mutex mtxLogger {};
Logger logger {
	.level = LogLevel_Trace,
	.prefix = {},
	.out = NULL,
	.mtx = &mtxLogger};
	
bool OpenLogger(LogLevel level, LogOutput logOutput, const char* filename) {
	logger.level = level;
	
	if (logOutput == LogOutput_Stdout) {
		logger.out = stdout;
	
	} else if (logOutput == LogOutput_Temporary) {
		
		SYSTEMTIME systemTime {0};
		GetSystemTime(&systemTime);
	
		char fileName[_MAX_PATH] {0};
		sprintf_s(fileName, ".\\%2d%2d_jed.tmp.log", systemTime.wHour, systemTime.wMinute);
		
        HANDLE hFile = CreateFileA(fileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	    if (hFile == INVALID_HANDLE_VALUE) {
		    fputs("CreateFileA() failed", stderr);
		    return false;
	    }
	    
	    int fileDescriptor = _open_osfhandle(reinterpret_cast<intptr_t>(hFile), _O_RDWR);
    	if (fileDescriptor == -1) {
        	CloseHandle(hFile);
        	fputs("_open_osfhandle() failed", stderr);
        	return false;
    	}

    	FILE* file = _fdopen(fileDescriptor, "w");
    	if (!file) {
    		_close(fileDescriptor);
    		fputs("_fdopen() failed", stderr);
    		return false;
		}
    	
		logger.out = file;
	
	} else if (logOutput == LogOutput_File) {
		char fileNameBuffer[_MAX_PATH] {0};
		if (!filename) {
			SYSTEMTIME systemTime {0};
			GetSystemTime(&systemTime);

			sprintf_s(fileNameBuffer, ".\\%2d%2d_jed.tmp.log", systemTime.wHour, systemTime.wMinute);
			
			filename = fileNameBuffer;
		}
		
		FILE* file = NULL;
		const errno_t err = fopen_s(&file, filename, "a");
		if (err != 0 || !file) {
			fputs("fopen() failed", stderr);
			return false;
		}
		
		logger.out = file;
	
	} else return false;
	
	return true;
}

void CloseLogger() {
	fflush(logger.out);	
	if (logger.out != stdout)
		fclose(logger.out);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void LogDev(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Dev, fmt, args);
	va_end(args);
} 
void LogFatal(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Fatal, fmt, args);
	va_end(args);
}
void LogError(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Error, fmt, args);
	va_end(args);
}
void LogWarning(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Warning, fmt, args);
	va_end(args);
}
void LogInfo(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Info, fmt, args);
	va_end(args);
}
void LogTrace(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger.Log(LogLevel_Trace, fmt, args);
	va_end(args);
}

void LogDev(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Dev, fmt, args);
	va_end(args);
} 
void LogFatal(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Fatal, fmt, args);
	va_end(args);
}
void LogError(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Error, fmt, args);
	va_end(args);
}
void LogWarning(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Warning, fmt, args);
	va_end(args);
}
void LogInfo(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Info, fmt, args);
	va_end(args);
}
void LogTrace(Logger* logger, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logger->Log(LogLevel_Trace, fmt, args);
	va_end(args);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static const char* CommonHResultString(HRESULT hResult) {
	switch (hResult) {
		case S_OK: return " (S_OK)";
		case E_ABORT: return " (E_ABORT)";
		case E_ACCESSDENIED: return " (E_ACCESSDENIED)";
		case E_HANDLE: return " (E_HANDLE)";
		case E_INVALIDARG: return " (E_INVALIDARG)";
		case E_NOINTERFACE: return " (E_NOINTERFACE)";
		case E_NOTIMPL: return " (E_NOTIMPL)";
		case E_OUTOFMEMORY: return " (E_OUTOFMEMORY)";
		case E_UNEXPECTED: return " (E_UNEXPECTED)";
		default: return "";
	};
}

static const char* WaitResultString(DWORD res) {
	switch (res) {
		case WAIT_ABANDONED: return "(WAIT_ABANDONED)";
		case WAIT_OBJECT_0: return "(WAIT_OBJECT_0)";
		case WAIT_TIMEOUT: return "(WAIT_TIMEOUT)";
		case WAIT_FAILED: return "(WAIT_FAILED)";
		default: return "";
	}
}

static char printBuffer[256] {0};

const char* StrHr(long hResult) {
	memset(printBuffer, 0, sizeof(printBuffer));
	const _com_error comError {hResult};
	sprintf_s(printBuffer, "0x%x %s %s", hResult, CommonHResultString(hResult), comError.ErrorMessage());
	return printBuffer;
}

const char* StrLastErr(unsigned long lastErr) {
	memset(printBuffer, 0, sizeof(printBuffer));
	const _com_error comError {HRESULT_FROM_WIN32(lastErr)};
	sprintf_s(printBuffer, "%d %s", lastErr, comError.ErrorMessage());
	return printBuffer;
}

const char* StrWaitRes(unsigned long waitRes) {
	memset(printBuffer, 0, sizeof(printBuffer));
	const char* str = WaitResultString(waitRes);
	sprintf_s(printBuffer, "0x%x %s", waitRes, str);
	return printBuffer;
}

const char* Str(const std::from_chars_result& fcr) {
	memset(printBuffer, 0, sizeof(printBuffer));
	const std::error_code code = std::make_error_code(fcr.ec);
	sprintf_s(printBuffer, "(%d) %s", code.value(), code.message().c_str());
	return printBuffer;
}

const char* Str(const std::to_chars_result& tcr) {
	memset(printBuffer, 0, sizeof(printBuffer));
	const std::error_code code = std::make_error_code(tcr.ec);
	sprintf_s(printBuffer, "(%d) %s", code.value(), code.message().c_str());
	return printBuffer;
}

const char* Str(const toml::source_region& srcRegion) {
	memset(printBuffer, 0, sizeof(printBuffer));
	sprintf_s(printBuffer, "%s%s%d:%d-%d:%d",
		srcRegion.path ? srcRegion.path->c_str() : "",
		srcRegion.path ? ": " : "",
		srcRegion.begin.line,
		srcRegion.begin.column,
		srcRegion.end.line,
		srcRegion.end.column);
	return printBuffer;
}
