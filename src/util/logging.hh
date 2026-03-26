#pragma once
#include <charconv>
#include <mutex>
#include "util/format.hh"

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
#define MeasureTime(label, statment) {\
	LARGE_INTEGER _before, _after, _freq;\
	QueryPerformanceFrequency(&_freq);\
	QueryPerformanceCounter(&_before);\
	statment;\
	QueryPerformanceCounter(&_after);\
	LogDev("%: %ms", label, static_cast<f64>((_after.QuadPart - _before.QuadPart)) / (_freq.QuadPart / 1000.0));\
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace toml { struct source_region; }
struct RegexError;

FormatArgument FHr(long hResult);
FormatArgument FLastErr(unsigned long err);
FormatArgument FWaitRes(unsigned long waitRes);
FormatArgument F(const std::from_chars_result& fcr);
FormatArgument F(const std::to_chars_result& tcr);
FormatArgument F(const toml::source_region& srcRegion);
