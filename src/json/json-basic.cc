#include "json-basic.hh"
#include "util/logging.hh"
#include "util/file-util.hh"

#include <cJSON/cJSON.h>

JsonTrace::JsonTrace(const JsonTrace* prevTrace, std::string_view prop) noexcept
	: prev(prevTrace)
	, property(prop.data())
	, propertyLen(prop.size()) {}

JsonTrace::JsonTrace(const JsonTrace* prevTrace, u64 index) noexcept
	: prev(prevTrace)
	, property(nullptr)
	, index(index) {}

FormatArgument MakeFormatArg(const JsonTrace* trace) {
	return FormatArgument {
		.userdata = trace,
		.Write = [] (const void* userdata, std::ostream* sink) {
			auto trace = static_cast<const JsonTrace*>(userdata);

			std::string path {};
			while (trace) {

				if (trace->property) {
					path.reserve(path.size() + trace->propertyLen + 1);
					path.insert(path.begin(), trace->property, trace->property + trace->propertyLen);
					path.insert(path.begin(), '.');

				} else {
					auto indexAsString = std::to_string(trace->index);
					path.reserve(path.size() + indexAsString.size() + 1);
					path.insert(path.begin(), indexAsString.begin(), indexAsString.end());
					path.insert(path.begin(), '.');
				}

				trace = trace->prev;
			}

			(*sink) << "(root)" << path;
		}};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
thread_local JsonAllocator* activeJsonAllocator = nullptr;

static void* JsonMalloc(usize size) {
	if (activeJsonAllocator) {
		return (size == sizeof(cJSON))
			? static_cast<void*>(activeJsonAllocator->AllocateNode())
			: static_cast<void*>(activeJsonAllocator->AllocateString(size));

	} else {
		return malloc(size);
	}
}

static void JsonFree(void* memblock) {
	if (!activeJsonAllocator)
		free(memblock);
}


bool InitJsonLib() {
	cJSON_Hooks hooks {
		.malloc_fn = JsonMalloc,
		.free_fn = JsonFree };
	cJSON_InitHooks(&hooks);
	return true;
}

cJSON* JsonAllocator::AllocateNode() {
	
	if (nodesOccupied < nodesCapacity) {
	
		cJSON* slot = &nodePool[nodesOccupied];
		nodesOccupied++;
		return slot;
	
	} else {
		nodesOccupied++; // increase anyway - for statistics
		return static_cast<cJSON*>(AllocateMemory(sizeof(cJSON)));
	}
}

char* JsonAllocator::AllocateString(u64 size) {
	
	if ((stringOccupied + size) <= stringCapacity) {
		char* str = &stringPool[stringOccupied];
		stringOccupied += size;
		return str;
	
	} else {
		// unlike in AllocateNode, we do not increase occupied here because maybe we can fit the next allocation
		return static_cast<char*>(AllocateMemory(size));
	}
}

void* JsonAllocator::AllocateMemory(u64 size) {
	const usize actualSize = sizeof(Memblock) + (size - 1);
	auto block = static_cast<Memblock*>(malloc(actualSize));
	block->prev = memblocks;
	memblocks = block;

	return &block->data;
}

void JsonAllocator::Init(u64 nodeCapa /*= JSON_ALLOCATOR_DEFAULT_NODE_CAPACITY*/, u64 stringCapa /*= JSON_ALLOCATOR_DEFAULT_STRING_CAPACITY*/) {
	nodesCapacity = nodeCapa;
	nodePool = new cJSON[nodeCapa];
	
	stringCapacity = stringCapa;
	stringPool = new char[stringCapa];
}

static u64 ClearMemblocks(JsonAllocator* self) {
	
	u64 count = 0u;

	JsonAllocator::Memblock* block = self->memblocks;
	while (block) {
		JsonAllocator::Memblock* prev = block->prev;
		free(block);
		block = prev;
		count++;
	}
	self->memblocks = nullptr;
	return count;
}

void JsonAllocator::Shutdown() {
	ClearMemblocks(this);
	delete[] stringPool;
	delete[] nodePool;
}

JsonAllocator::~JsonAllocator() noexcept {
	Shutdown();
}

void JsonAllocator::Reset() {
	const u64 statMemblocks = ClearMemblocks(this);
	const u64 statNodes = nodesOccupied;
	const u64 statString = stringOccupied;
	
	memset(nodePool,   0, sizeof(cJSON) * nodesCapacity);
	memset(stringPool, 0, sizeof(char)  * stringCapacity);
	
	nodesOccupied = 0u;
	stringOccupied = 0u;
	
	LogDetail("allocator: node: %/% string: %/%  memblocks: %", statNodes, nodesCapacity, statString, stringCapacity, statMemblocks);
}

JsonAllocator* SetActiveJsonAllocator(JsonAllocator* newAllocator) {
	auto old = activeJsonAllocator;
	activeJsonAllocator = newAllocator;
	return old;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

cJSON* JsonParseFile(std::string_view pathToFile, bool stripComments) {
	std::string fileBuffer {};
	if (!ReadEntireFile(pathToFile, &fileBuffer))
		return nullptr;
		
	return JsonParseString(fileBuffer, stripComments);
}

cJSON* JsonParseString(std::string_view orginalBuffer, bool stripComments) {
	
	std::string buffer {orginalBuffer};
		
	//
	// strip comments
	//
	if (stripComments) {
	
		// @NOTE
		// we allow single line comments ("//") and multiline comments ("/* */")
		// even though multiline comments may change the line number in case of an error
		// 
		// This entire routine isn't very robust, for example we do not check for escape sequences
		// but refining it more doesn't make a lot of sense - this time would be better inveseted in
		// writing an full json parser that respects the comments
	
		for (u64 i = 0u; i < buffer.size() - 1u; i++) {
			
			if (buffer[i] == '/' && buffer[i+1] == '/') {
				
				const u64 commentStart = i;
				u64 commentLen = std::string::npos;
				
				for (u64 j = i+2; j < buffer.size(); j++) {
					if (buffer[j] == '\n') {
						commentLen = j - i;
						break;
					}
				}
				
				buffer.erase(commentStart, commentLen); 
				i += commentLen;	
			
			} else if (buffer[i] == '/' && buffer[i+1] == '*') {
			
				const u64 commentStart = i;
				u64 commentLen = std::string::npos;
				
				for (u64 j = i+2; j < buffer.size() - 1; j++) {
					if (buffer[j] == '*' && buffer[j+1] == '/') {
						commentLen = j - i + 1;
						break;
					}
				}
				
				if (commentLen != std::string::npos) {
					buffer.erase(commentStart, commentLen); 
					i += commentLen;	
				}
			
			} else if (buffer[i] == '"') {
				
				i++; // skip this '"'
				while (buffer[i] != '"' && i < buffer.size())
					i += 1;
			}
		}
	}
	
	//
	// parse json
	//
	const char* errpos = nullptr;
	cJSON* result = cJSON_ParseWithLengthOpts(buffer.data(), buffer.size(), &errpos, 0);
	
	//
	// print error
	//
	if (!result) {
		
		const char* begin = buffer.data();
		const char* end = buffer.data() + buffer.size();

		// we start with one because users don't want to see 0-based line numbers
		int linenr = 1;
		
		const char* linestart = begin;
		for (const char* curr = begin; curr < errpos; curr++) {
			if (*curr == '\n') {
				linestart = curr + 1;
				linenr++;
			}
		}

		const char* lineend = end;
		for (const char* curr = errpos; curr < end; curr++) {
			if (*curr == '\n') {
				lineend = curr;
				break;
			}
		}

		const std::string_view textBeforeError {linestart, errpos};
		const std::string_view textAfterError  {errpos, lineend};

		LogError("Parse error at line %\n"
				 "       %\x1b[4m%\x1b[0m", linenr, textBeforeError, textAfterError);
				//^^^^^^^^
				// we leave a little extra space here because we have no loglevel prefix
				// and this way it nicely aligns with the other log messages		
	}
	
	return result;
}

void JsonFree(cJSON* json) {
	cJSON_free(json);
}

std::string_view JsonTypeToString(int type){
	switch (type & 0xFF) {
		case cJSON_Invalid: return "invalid";
		case cJSON_False:   return "boolean";
		case cJSON_True:    return "boolean";
		case cJSON_NULL:    return "null";
		case cJSON_Number:  return "number";
		case cJSON_String:  return "string";
		case cJSON_Array:   return "array";
		case cJSON_Object:  return "object";
		case cJSON_Raw:     return "raw";
		default:            return "unknown";
	}	
}