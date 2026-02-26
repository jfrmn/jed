#include "json-mapping.hh"

#include <cJSON/cJSON.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool JsonObjectToMap(const JsonTrace* trace, const cJSON* json, /*out*/ std::unordered_map<std::string_view, cJSON*>* map) {
	if (!JsonCheckType(trace, json, cJSON_Object)) return false;
	
	for (cJSON* node = json->child; node != nullptr; node = node->next)
		map->emplace(node->string, node);

	return true;
}

bool JsonArrayToVector(const JsonTrace* trace, const cJSON* json, /*out*/ std::vector<cJSON*>* vec) {
	if (!JsonCheckType(trace, json, cJSON_Array)) return false;
	
	for (cJSON* node = json->child; node != nullptr; node = node->next)
		vec->push_back(node);
		
	return true;
}

bool JsonCheckType(const JsonTrace* trace, const cJSON* json, int expectedTypes) {
	
	// 256 = IsReference; 512 = StringIsConst - we don't care about these and need to filter them out
	const int actualType = json->type & 0xFF;
	
	if ((actualType & expectedTypes) != 0)
		return true;
	
	std::string acceptedTypesAsStr {};
	
	for (int i = 0; i < 8; i++) {
		const int typeToTest = (1 << i);
		if ((expectedTypes & typeToTest) == typeToTest) {
			
			if (!acceptedTypesAsStr.empty()) acceptedTypesAsStr.append(" or");
			acceptedTypesAsStr.push_back('[');
			acceptedTypesAsStr.append(JsonTypeToString(typeToTest));
			acceptedTypesAsStr.push_back(']');
		}
	}
	
	JsonLogError(trace, "expected % - but was [%]", acceptedTypesAsStr, JsonTypeToString(json->type));
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<class T>
static bool JsonToNumberValue(const JsonTrace* trace, const cJSON* json, T* result) {
	if (!JsonCheckType(trace, json, cJSON_Number)) return false;
	*result	= static_cast<T>(cJSON_GetNumberValue(json));
	return true;
}

template<class T>
static bool JsonToStringValue(const JsonTrace* trace, const cJSON* json, T *result) {
	if (!JsonCheckType(trace, json, cJSON_String)) return false;
	*result = json->valuestring;
	return true;
}

// signed int
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s8* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s16* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s32* result) {
	return JsonToNumberValue(trace, json, result);
}

bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ s64* result) {
	return JsonToNumberValue(trace, json, result);
}

// unsigned int
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u8* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u16* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u32* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ u64* result) {
	return JsonToNumberValue(trace, json, result);
}

// float
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ f32* result) {
	return JsonToNumberValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ f64* result) {
	return JsonToNumberValue(trace, json, result);
}

// bool
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ bool* result){
	if (!JsonCheckType(trace, json, cJSON_True | cJSON_False)) return false;
	*result = cJSON_IsTrue(json);
	return true;
}

// string
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ char* result) {
	if (!JsonCheckType(trace, json, cJSON_String)) return false;
	ASSERT(json->valuestring);
	*result = *json->valuestring;
	return true;
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::string_view* result) {
	return JsonToStringValue(trace, json, result);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::string* result) {
	return JsonToStringValue(trace, json, result);
}

// misc
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ std::nullptr_t result) {
	return JsonCheckType(trace, json, cJSON_NULL);
}
bool JsonToValue(const JsonTrace* trace, const cJSON* json, /*out*/ _D3DCOLORVALUE* result) {
	return false; // @TODO
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cJSON* JsonFromValue(s8  input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(s16 input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(s32 input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(s64 input) { return cJSON_CreateNumber(static_cast<double>(input)); }

// uint
cJSON* JsonFromValue(u8  input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(u16 input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(u32 input) { return cJSON_CreateNumber(static_cast<double>(input)); }
cJSON* JsonFromValue(u64 input) { return cJSON_CreateNumber(static_cast<double>(input)); }

// float
cJSON* JsonFromValue(f32 input) { return cJSON_CreateNumber(input); }
cJSON* JsonFromValue(f64 input) { return cJSON_CreateNumber(input); }

// bool
cJSON* JsonFromValue(bool input) { return cJSON_CreateBool(input ? 1 : 0); }

// string
cJSON* JsonFromValue(char input)              { char buffer[2] {input, '\0'}; return cJSON_CreateString(buffer); }
cJSON* JsonFromValue(wchar input)             { return JsonFromValue(static_cast<char>(input)); } // @TODO(utf-8)
cJSON* JsonFromValue(std::string_view input)  { return cJSON_CreateStringReference(input.data()); }

// misc
cJSON* JsonFromValue(std::nullptr_t input) { return cJSON_CreateNull(); }