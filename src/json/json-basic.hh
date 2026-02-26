#pragma once
#include "basic.hh"
#include "util/logging.hh"
#include <string>

struct cJSON;

//-----------------------------------------------------------------------------
// JsonTrace
//-----------------------------------------------------------------------------

struct JsonTrace {
	explicit JsonTrace(const JsonTrace* prevTrace, std::string_view prop) noexcept;
	explicit JsonTrace(const JsonTrace* prevTrace, u64 index) noexcept;

	const JsonTrace* prev = nullptr;
	const char* property  = nullptr;
	union {
		usize propertyLen = 0;
		usize index;
	};
};

FormatArgument MakeFormatArg(const JsonTrace* trace);

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

template<bool ok, class...Args>
void JsonLogChecked(const JsonTrace* trace, LogLevel lvl, std::string_view fmt, Args&&...args) {
	static_assert(ok, "Provided arguments != arguments required by the format string");
	
	const std::string_view tracePrefix = "\x1b[90m%\x1b[0m: ";
	std::string format{};
	format.reserve(tracePrefix.size() + fmt.size());
	format.append(tracePrefix);
	format.append(fmt);

	const FormatArgument arguments[] { MakeFormatArg(trace), MakeFormatArg(&args)...};
	activeLogger->LogWithArgs(lvl, format, std::span {arguments});
}

#define JsonLogWarning(trace, fmt, ...) JsonLogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(trace, LogLevel_Warning, fmt __VA_OPT__(,) __VA_ARGS__)
#define JsonLogError(trace, fmt, ...)   JsonLogChecked<CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>(trace, LogLevel_Error,   fmt __VA_OPT__(,) __VA_ARGS__)

//-----------------------------------------------------------------------------
// Allocator
//-----------------------------------------------------------------------------

// sets the malloc- and free-functions for cJSON
bool InitJsonLib();

#define JSON_ALLOCATOR_DEFAULT_NODE_CAPACITY   32
#define JSON_ALLOCATOR_DEFAULT_STRING_CAPACITY 256

struct JsonAllocator {
	
	//-------------------------------------------
	// types
	
	struct Memblock {
		Memblock* prev = nullptr;
		u8 data[1];
	};
	
	//-------------------------------------------
	// data
	
	u64 nodesCapacity = 0u;
	u64 nodesOccupied = 0u;
	
	u64 stringCapacity = 0u;
	u64 stringOccupied = 0u;
	
	cJSON* nodePool  = nullptr;
	char* stringPool = nullptr;
	
	Memblock* memblocks = nullptr;
	
	//-------------------------------------------
	// functions
	
	cJSON* AllocateNode();
	char*  AllocateString(u64 size);
	void*  AllocateMemory(u64 size);

	void Init(u64 nodeCapa = JSON_ALLOCATOR_DEFAULT_NODE_CAPACITY, u64 stringCapa = JSON_ALLOCATOR_DEFAULT_STRING_CAPACITY);
	void Shutdown();
	~JsonAllocator() noexcept;
	
	void Reset();
};

extern thread_local JsonAllocator* activeJsonAllocator;
JsonAllocator* SetActiveJsonAllocator(JsonAllocator* newAllocator);

//-----------------------------------------------------------------------------
// Misc
//-----------------------------------------------------------------------------

cJSON* JsonParseFile(std::string_view pathToFile, bool stripComments);
cJSON* JsonParseString(std::string_view buffer,   bool stripComments);
void JsonFree(cJSON* json);

std::string_view JsonTypeToString(int type);


