#pragma once
#include "basic.hh"

#include <string>
#include <string_view>
#include <span>
#include <unordered_map>

#include <cJSON.h>

//=============================================================================
// JsonWriteBuffer
//=============================================================================

struct JsonWriteBuffer {
	
	//------------------------------------------
	// data
	//------------------------------------------
	
	char* begin = nullptr;
	char* end = nullptr;
	char* head = nullptr;
	bool reallocated = false;
	
	//------------------------------------------
	// functions
	//------------------------------------------

	explicit JsonWriteBuffer(std::span<char> staticBuffer) noexcept;
	~JsonWriteBuffer() noexcept;
	
	void WriteObjectStart();
	void WriteObjectEnd();
	
	void WriteArrayStart();
	void WriteArrayEnd();
	
	JsonWriteBuffer* WriteProperty(std::string_view property);
		
	void WriteNull();
	void WriteBoolean(bool value);
	void WriteUnsigned(u64 value);
	void WriteInteger(int value);
	void WriteFloat(f64 value);
	void WriteString(std::string_view value);
	void WriteRawString(std::string_view value);
	
	void WriteEnumString(std::string_view enumValue);
	
	template<class T>
	void WriteCustom(const T* value) {
		WriteJson(value, this);
	}
	
	void Put(char ch);		
	std::string_view GetString() const;
};

//=============================================================================
// JsonObjectReader
//=============================================================================

struct cJSON;

struct JsonObjectReader {

	//------------------------------------------
	// data
	//------------------------------------------
	
	std::unordered_map<std::string_view, cJSON*> properties = {};
	const cJSON* object = nullptr;
	bool ok = true;

	//------------------------------------------
	// functions
	//------------------------------------------
	
	explicit JsonObjectReader(const cJSON* object);
	
	bool ReadUnsigned(std::string_view property, u64* value);
	bool ReadInteger(std::string_view property, int* value);
	bool ReadFloat(std::string_view property, f32* value);
	bool ReadString(std::string_view property, std::string_view* value);
	bool ReadBoolean(std::string_view property, bool* value);
	
	template<class T>
 	bool ReadValue(std::string_view property, T* value) {
		const cJSON* json = Get(property);
		if (!json) return false;
		ReadJson(json, value);
		return true;
	}
	
	bool Contains(std::string_view property) const;
	const cJSON* GetObject(std::string_view property);
	const cJSON* GetArray(std::string_view property);
	const cJSON* Get(std::string_view property);
};

//=============================================================================
// JsonAllocator
//=============================================================================

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
	
	static thread_local JsonAllocator* activeAllocator;
	
	void Init();
	void Shutdown();
	~JsonAllocator() noexcept;
	
	cJSON* AllocateNode();
	char*  AllocateString(u64 size);
	void*  AllocateMemory(u64 size);
	
	void Reset();
};

//=============================================================================
// Free Functions
//=============================================================================

std::string_view JsonTypeToString(int type);
bool JsonExpectType(const cJSON* json, int type, /*out*/ std::string* errorStr = nullptr);
bool InitJsonLib();
