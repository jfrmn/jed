#pragma once
#include <charconv>
#include <mutex>
#include <string_view>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum LogLevel {
	 LogLevel_Off = 0,
	 LogLevel_Dev,
	 LogLevel_Fatal,
	 LogLevel_Error,
	 LogLevel_Warning,
	 LogLevel_Info,
	 LogLevel_Trace
};

struct Logger {
	LogLevel level          = LogLevel_Off;
	std::string_view prefix = {};
	FILE* out               = nullptr;
	std::mutex* mtx         = nullptr;

	void Log(LogLevel level, const char* fmt, va_list list);
};

extern Logger logger;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void LogDev(const char* fmt, ...);
void LogFatal(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogWarning(const char* fmt, ...);
void LogInfo(const char* fmt, ...);
void LogTrace(const char* fmt, ...);

void LogDev(Logger* logger, const char* fmt, ...);
void LogFatal(Logger* logger, const char* fmt, ...);
void LogError(Logger* logger, const char* fmt, ...);
void LogWarning(Logger* logger, const char* fmt, ...);
void LogInfo(Logger* logger, const char* fmt, ...);
void LogTrace(Logger* logger, const char* fmt, ...);

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

const char* StrHr(long hResult);
const char* StrLastErr(unsigned long err);
const char* StrWaitRes(unsigned long waitRes);
const char* Str(const std::from_chars_result& fcr);
const char* Str(const std::to_chars_result& tcr);
const char* Str(const toml::source_region& srcRegion);
#define SIZE_AND_DATA(sv) static_cast<int>(sv.size()), sv.data()