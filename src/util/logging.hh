#pragma once
#include "util/format.hh"
#include <mutex>
#include <charconv>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum LogLevel {
	 LogLevel_Off = 0,
	 LogLevel_Dev,
	 LogLevel_Fatal,
	 LogLevel_Error,
	 LogLevel_Warning,
	 LogLevel_Info,
	 LogLevel_Detail
};

struct Logger {
	LogLevel level          = LogLevel_Off;
	std::string_view prefix = {};
	std::ostream *out       = nullptr;
	std::mutex   *mtx       = nullptr;

	void LogWithArgs(LogLevel level, std::string_view fmt, std::span<const FormatArgument> args);

	template<bool ok, class ...Args>
	void LogChecked(LogLevel level, std::string_view fmt, Args&&...args) {
		static_assert(ok, "Provided arguments != arguments required by the format string");
		const FormatArgument arguments[] {MakeFormatArg(&args)...};
		LogWithArgs(level, fmt, std::span {arguments});
	}
	
	template<bool ok>
	void LogChecked(LogLevel level, std::string_view fmt) {
		static_assert(ok, "Provided arguments != arguments required by the format string");
		LogWithArgs(level, fmt, {});
	}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
extern thread_local Logger* activeLogger;
Logger* SetActiveLogger(Logger* logger);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define LogDev(fmt, ...)     activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Dev,     fmt __VA_OPT__(,) __VA_ARGS__)
#define LogFatal(fmt, ...)   activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Fatal,   fmt __VA_OPT__(,) __VA_ARGS__)
#define LogError(fmt, ...)   activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Error,   fmt __VA_OPT__(,) __VA_ARGS__)
#define LogWarning(fmt, ...) activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Warning, fmt __VA_OPT__(,) __VA_ARGS__)
#define LogInfo(fmt, ...)    activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Info,    fmt __VA_OPT__(,) __VA_ARGS__)
#define LogDetail(fmt, ...)  activeLogger->LogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(LogLevel_Detail,  fmt __VA_OPT__(,) __VA_ARGS__)

#define LogDevVar(varname)   activeLogger->LogChecked<true>(LogLevel_Dev, "%: %", #varname, varname)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct _GUID;

FormatArgument FHr(long hResult);
FormatArgument FLastErr(unsigned long err);
FormatArgument FWaitRes(unsigned long waitRes);
FormatArgument FFromCharsResult(const std::from_chars_result& fcr);
FormatArgument FToCharsResult(const std::to_chars_result& tcr);
FormatArgument FGuid(const _GUID& guid);