#pragma once
#include "json-basic.hh"

#include <unordered_map>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <memory>

#include <cJSON/cJSON.h>

// These functions are seperated into their own file
// so that we don't recursivly include a bunch of stl-stuff
// that we neither need nor want most of the time

struct cJSON;

//=============================================================================
// JsonToValue
//=============================================================================

template<class T>
bool JsonToValue(const JsonTrace* trace, const cJSON* json, std::unique_ptr<T>* result);

template<class T>
bool JsonToValue(const JsonTrace* trace, const cJSON* json, std::vector<T>* result);

template<class T, size_t N>
bool JsonToValue(const JsonTrace* trace, const cJSON* json, const std::array<T, N>* result);

template<class Tv>
bool JsonToValue(const JsonTrace* trace, const cJSON* json, std::unordered_map<std::string, Tv>* result);

//=============================================================================
// JsonFromValue
//=============================================================================

template<class T>
cJSON* JsonFromValue(const std::unique_ptr<T>& input);

template<class T>
cJSON* JsonFromValue(const std::optional<T>& input);

template<class T>
cJSON* JsonFromValue(const std::vector<T>& input);

template<class T, size_t N>
cJSON* JsonFromValue(const std::array<T, N>& input);

template<class Tv, class Thash, class Teq, bool copyKey = false>
cJSON* JsonFromValue(const std::unordered_map<std::string, Tv, Thash, Teq>& input);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Implementation
//

//=============================================================================
// JsonToValue
//=============================================================================

template<class T>
inline bool JsonToValue(const JsonTrace* trace, const cJSON* json, std::unique_ptr<T>* result) {
	
	auto ptr = std::make_unique<T>();
	if (!JsonToValue(trace, json, ptr.get()))
		return false;

	*result = std::move(ptr);
	return true;	
}

template<class T>
inline bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, std::vector<T>* result) {

	if (!JsonCheckType(parentTrace, json, cJSON_Array))
		return false;

	for (cJSON* node = json->child; node != nullptr; node = node->next) {
		const JsonTrace trace {parentTrace, result->size()};

		if (!JsonToValue(&trace, node, &result->emplace_back()))
			return false;
	}

	return true;
}

template <class T, size_t N>
inline bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, const std::array<T, N>* result) {
	
	if (!JsonCheckType(parentTrace, json, cJSON_Array))
		return false;

	const int arraySize = cJSON_GetArraySize(json);
	if (arraySize != N)
		JsonLogWarning(parentTrace, "array size missmatch: expected % values but got %", N, arraySize);
	
	for (u64 i = 0u; i < arraySize; i++) {
		const JsonTrace trace {parentTrace, i};
		
		if (!JsonToValue(&trace, json, &result[i]))
			return false;
	}

	return true;
}

template<class Tv>
inline bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, std::unordered_map<std::string, Tv>* result) {
	
	if (!JsonCheckType(parentTrace, json, cJSON_Object)) return false;

	for (cJSON* node = json->child; node != nullptr; node = node->next) {
		const JsonTrace jsonTrace {parentTrace, node->string};
		
		Tv value;
		if (!JsonToValue(&jsonTrace, node, &value))
			return false;

		result->emplace(node->string, std::move(value));
	}

	return true;
}


//=============================================================================
// JsonFromValue
//=============================================================================

template<class T>
inline cJSON* JsonFromValue(const std::unique_ptr<T>& input) {
	return input
		? JsonFromValue(*input.get())
		: cJSON_CreateNull();
}

template<class T>
inline cJSON* JsonFromValue(const std::optional<T>& input) {
	return input.has_value()
		? JsonFromValue(input.value())
		: cJSON_CreateNull();
}

template<class T>
cJSON* JsonFromArray(const T* data, size_t size) {

	cJSON* jsonArray = cJSON_CreateArray();
	if (!jsonArray) {
		LogError("failed allocate node");
		return nullptr;
	}
	
	for (size_t i = 0u; i < size; i++) {

		const T& item = data[i];
		if (cJSON* jsonItem = JsonFromValue(item)) {
			cJSON_AddItemToArray(jsonArray, jsonItem);

	 	} else {
			return nullptr;
		}
	}

	return jsonArray;
}

template<class T> 
inline cJSON* JsonFromValue(const std::vector<T>& input) {
	return JsonFromArray(input.data(), input.size());
}

template<class T, size_t N> 
inline cJSON* JsonFromValue(const std::array<T, N>& input) {
	return JsonFromArray(input.data(), input.size());
}

template<class Tv, class Thash, class Teq, bool copyKey>
inline cJSON* JsonFromValue(const std::unordered_map<std::string, Tv, Thash, Teq>& input) {
	
	cJSON* nodeObject = cJSON_CreateObject();
	if (!nodeObject) {
		LogError("failed to allocate node");
		return nullptr;
	}
	
	for (auto it = input.begin(); it != input.end(); ++it) {

		const JsonTrace trace {it->second};

		cJSON *nodeItem = ToJson(it->second);
		if (!nodeItem) {
			LogError("json: failed to serialize map-item '$'", it->first);
			return nullptr;
		}

		if constexpr (copyKey) {
			cJSON_AddItemToObject(nodeObject, it->first.c_str(), nodeItem);
		} else {
			cJSON_AddItemToObjectCS(nodeObject, it->first.c_str(), nodeItem);
		}
	}

	return nodeObject;
}
