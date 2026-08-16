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
static std::mutex mtxCout {};
Logger logger {
	.level = LogLevel_Trace,
	.prefix = {},
	.out = stdout,
	.mtx = &mtxCout};

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

const char* StrHr(long hResult) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	const _com_error comError {hResult};
	sprintf_s(buffer, "0x%x %s %s", hResult, CommonHResultString(hResult), comError.ErrorMessage());
	return buffer;
}

const char* StrLastErr(unsigned long lastErr) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	const _com_error comError {HRESULT_FROM_WIN32(lastErr)};
	sprintf_s(buffer, "%d %s", lastErr, comError.ErrorMessage());
	return buffer;
}

const char* StrWaitRes(unsigned long waitRes) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	const char* str = WaitResultString(waitRes);
	sprintf_s(buffer, "0x%x %s", waitRes, str);
	return buffer;
}

const char* Str(const std::from_chars_result& fcr) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	const std::error_code code = std::make_error_code(fcr.ec);
	sprintf_s(buffer, "(%d) %s", code.value(), code.message().c_str());
	return buffer;
}

const char* Str(const std::to_chars_result& tcr) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	const std::error_code code = std::make_error_code(tcr.ec);
	sprintf_s(buffer, "(%d) %s", code.value(), code.message().c_str());
	return buffer;
}

const char* Str(const toml::source_region& srcRegion) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));
	sprintf_s(buffer, "%s%s%d:%d-%d:%d",
		srcRegion.path ? srcRegion.path->c_str() : "",
		srcRegion.path ? ": " : "",
		srcRegion.begin.line,
		srcRegion.begin.column,
		srcRegion.end.line,
		srcRegion.end.column);
	return buffer;
}
