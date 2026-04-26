#include "logging.hh"
#include <iostream>

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
static std::mutex mtxCout {};
static Logger globalLogger {
	.level = LogLevel_Detail,
	.prefix = {},
	.out = &std::cout,
	.mtx = &mtxCout };
	
thread_local Logger* activeLogger = &globalLogger;

Logger* SetActiveLogger(Logger* logger) {
	Logger* tmp = activeLogger;
	activeLogger = logger;
	return tmp;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static const char* logLevelPrefix[] = {
	nullptr /* OFF */,
	"\x1b[35mDEV:    \x1b[0m",
    "\x1b[41mFATAL:  \x1b[0m",
	"\x1b[31mERROR:  \x1b[0m",
	"\x1b[33mWARN:   \x1b[0m",
	"\x1b[32mINFO:   \x1b[0m",
	"\x1b[34mDETAIL: \x1b[0m"
};

void Logger::LogWithArgs(LogLevel msgLevel, std::string_view fmt, std::span<const FormatArgument> args) {
	
	if (level >= msgLevel) {
		
		if (mtx)
			mtx->lock();

		(*out) << logLevelPrefix[msgLevel];
		
		if (!prefix.empty())
			(*out) << "\x1b[36m" << prefix << "\x1b[0m ";
		
		FormatWithArgs(out, fmt, args);

		(*out) << std::endl;

		if (mtx)
			mtx->unlock();
	}
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

#pragma warning (disable : 4312) // conversion from 'long' to 'void *' of greater size
#pragma warning (disable : 4311) // pointer truncation from 'const void *' to 'long'
#pragma warning (disable : 4302) // truncation from 'const void *' to 'long'

FormatArgument FHr(long hResult) {
	return FormatArgument {
		.userdata = reinterpret_cast<void*>(hResult),
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto hResult = reinterpret_cast<const long>(userdata);
			const _com_error comError {hResult};
			(*sink) << "0x" << std::hex << hResult << ' ' << CommonHResultString(hResult) << comError.ErrorMessage();
		}
	};
}

FormatArgument FLastErr(unsigned long waitRes) {
	return FormatArgument {
		.userdata = reinterpret_cast<void*>(waitRes),
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto lastErr = reinterpret_cast<const unsigned long>(userdata);
			const _com_error comError {HRESULT_FROM_WIN32(lastErr)};
			(*sink) << "0x" << std::hex << lastErr << ' ' << comError.ErrorMessage();
		}
	};
}

FormatArgument FWaitRes(unsigned long waitRes) {
	return FormatArgument {
		.userdata = reinterpret_cast<void*>(waitRes),
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto waitRes = reinterpret_cast<const unsigned long>(userdata);
			const std::string_view str = WaitResultString(waitRes);
			(*sink) << "0x" << std::hex << waitRes << ' ' << str;
		}
	};
}

FormatArgument F(const std::from_chars_result& fcr) {
	return FormatArgument {
		.userdata = &fcr,
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto fcr = static_cast<const std::from_chars_result*>(userdata);
			const std::error_code code = std::make_error_code(fcr->ec);
			(*sink) << '(' << code.value() << ") " << code.message();
		}
	};
}

FormatArgument F(const std::to_chars_result& tcr) {
	return FormatArgument {
		.userdata = &tcr,
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto fcr = static_cast<const std::from_chars_result*>(userdata);
			const std::error_code code = std::make_error_code(fcr->ec);
			(*sink) << '(' << code.value() << ") " << code.message();
		}
	};
}

FormatArgument F(const toml::source_region& srcRegion) {
	return FormatArgument {
		.userdata = &srcRegion,
		.Write = [] (const void* userdata, std::ostream* sink) {
			const auto srcReg = static_cast<const toml::source_region*>(userdata);
			(*sink) << (srcReg->path ? *srcReg->path : std::string {})
			        << ':' << srcReg->begin.line << ':' << srcReg->begin.column
			        << '-' << srcReg->end.line << ':' << srcReg->end.column;
		}
	};
}
