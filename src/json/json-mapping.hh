#pragma once
#include "basic.hh"
#include "json-basic.hh"

#include <string>
#include <vector>
#include <unordered_map>

struct cJSON;
struct _D3DCOLORVALUE;
struct JsonTrace;

//=============================================================================
// JsonToValue
//=============================================================================

bool JsonObjectToMap(  const JsonTrace* trace, const cJSON* json, /*out*/ std::unordered_map<std::string_view, cJSON*>* map);
bool JsonArrayToVector(const JsonTrace* trace, const cJSON* json, /*out*/ std::vector<cJSON*>* vec);
bool JsonCheckType(    const JsonTrace* trace, const cJSON* json, int expectedTypes);

//-----------------------------------------------------------------------------
// Primitives
//-----------------------------------------------------------------------------

// signed int
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s8*  result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s16* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s32* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s64* result);

// unsigned int
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u8*  result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u16* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u32* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u64* result);

// float
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ f32* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ f64* result);

// bool
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ bool* result);

// string
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ char*  result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ wchar* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::string_view* result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::wstring*     result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::string*      result);

// misc
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::nullptr_t result);
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ _D3DCOLORVALUE* result);

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

//
// structs
//
#define JSON_TO_VALUE_BEGIN(T)\
	bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, /*out*/ T* result) {\
		ASSERT(json && result);\
		\
		std::unordered_map<std::string_view, cJSON*> properties {};\
		if (!JsonObjectToMap(parentTrace, json, &properties)) return false;

#define JSON_TO_VALUE_PROPERTY_NAMED(propertyInJson, property)\
	if (auto node = properties.extract(propertyInJson); !node.empty()) {\
		const JsonTrace trace {parentTrace, propertyInJson};\
		\
		if (!JsonToValue(&trace, node.mapped(), &result->property)) return false;\
	}
	
#define JSON_TO_VALUE_PROPERTY(property)\
		JSON_TO_VALUE_PROPERTY_NAMED(#property, property);
		
#define JSON_TO_VALUE_PROPERTY_NAMED_REQUIRED(propertyInJson, property)\
	{\
		const JsonTrace trace {parentTrace, propertyInJson};\
		if (auto node = properties.extract(propertyInJson); !node.empty()) {\
			if (!JsonToValue(&trace, node.mapped(), &result->property)) return false;\
		} else {\
			JsonLogError(&trace, "required property is missing");\
		}\
	}
	
#define JSON_TO_VALUE_PROPERTY_REQUIRED(property)\
		JSON_TO_VALUE_PROPERTY_NAMED_REQUIRED(#property, property)
		
#define JSON_TO_VALUE_CHECK_UNRECOGNIZED\
		for (const auto& kvp : properties) {\
			const JsonTrace trace {parentTrace, kvp.first};\
			JsonLogWarning(&trace, "unrecognized property '%'", kvp.first);\
		}

#define JSON_TO_VALUE_END\
		return true;\
	}

//
// enums
//
#define JSON_TO_ENUM_BEGIN(T)\
	bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ T* result) {\
		ASSERT(json && result);\
		\
		const char* enumName = #T;\
		using enum T;\
		\
		std::string_view valueAsStr {};\
		if (!JsonToValue(trace, json, &valueAsStr)) return false;\
		\
		if (false) {
		
		
#define JSON_TO_ENUM_MEMBER(strInJson, enumMember)\
		} else if (valueAsStr == strInJson) {\
			*result = enumMember;\
			return true;\
			
#define JSON_TO_ENUM_END\
		} else {\
			JsonLogError(trace, "invalid value '%' for %", valueAsStr, enumName);\
			return false;\
		}\
	}


//
// (tagged) unions
//
#define JSON_TO_UNION_BEGIN_NAMED(T, tagInJson, tagProperty)\
	bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, /*out*/ T* result) {\
		ASSERT(json && result);\
		\
		std::unordered_map<std::string_view, cJSON*> properties {};\
		if (!JsonObjectToMap(parentTrace, json, &properties)) return false;\
		\
		JSON_TO_VALUE_PROPERTY_NAMED_REQUIRED(tagInJson, tagProperty)\
		const auto tag = result->tagProperty;\
		\
		if (false) {
		
#define JSON_TO_UNION_BEGIN(T, tagProperty)\
		JSON_TO_UNION_BEGIN_NAMED(T, #tagProperty, tagProperty)
		
 #define JSON_TO_UNION_PROPERTIES_FOR_TAG(tagValue)\
		} else if (tag == tagValue) {
	
#define JSON_TO_UNION_END\
		} else ASSERT_UNREACHABLE;\
		\
		return true;\
	}


//=============================================================================
// JsonFromValue
//=============================================================================

//-----------------------------------------------------------------------------
// Primitives
//-----------------------------------------------------------------------------

// int
cJSON* JsonFromValue(s8  input);
cJSON* JsonFromValue(s16 input);
cJSON* JsonFromValue(s32 input);
cJSON* JsonFromValue(s64 input);

// uint
cJSON* JsonFromValue(u8  input);
cJSON* JsonFromValue(u16 input);
cJSON* JsonFromValue(u32 input);
cJSON* JsonFromValue(u64 input);

// float
cJSON* JsonFromValue(f32 input);
cJSON* JsonFromValue(f64 input);

// bool
cJSON* JsonFromValue(bool input);

// string
cJSON* JsonFromValue(char input);
cJSON* JsonFromValue(wchar input);
cJSON* JsonFromValue(std::string_view input);

// misc
cJSON* JsonFromValue(std::nullptr_t input);

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

//
// structs
//
#define JSON_FROM_VALUE_BEGIN(T)\
	cJSON* JsonFromValue(const T& input) {\
	\
		cJSON* jsonObject = cJSON_CreateObject();
		
#define JSON_FROM_VALUE_PROPERTY_NAMED(propertyInJson, property)\
		{\
			cJSON* jsonProperty = JsonFromValue(input.property);\
			if (!jsonProperty) return nullptr;\
			\
			cJSON_AddItemToObjectCS(jsonObject, propertyInJson, jsonProperty);\
		}
		
#define JSON_FROM_VALUE_PROPERTY(property)\
		JSON_FROM_VALUE_PROPERTY_NAMED(#property, property)

#define JSON_FROM_VALUE_END\
		return jsonObject;\
	}

//
// enum
//
#define JSON_FROM_ENUM_BEGIN(T)\
	cJSON* JsonFromValue(const T& input) {\
		\
		constexpr auto enumName = #T;\
		using enum T;\
		\
		switch (input) {


#define JSON_FROM_ENUM_MEMBER(strInJson, enumMember)\
			case enumMember: return cJSON_CreateStringReference(strInJson);

#define JSON_FROM_ENUM_END\
			default: {\
				LogWarning("json: value % of enum '%' not mapped", static_cast<int>(input), enumName);\
				return JsonFromValue(static_cast<int>(input));\
			}\
		}\
	}
