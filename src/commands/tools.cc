#include "tools.hh"
#include "json/json-basic.hh"
#include "json/json-mapping.hh"
#include "json/json-mapping-stl.h"

#include <cJSON/cJSON.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::vector<Tool> Tool::tools {};

static bool CheckParameters(std::string_view toolName, std::span<const Parameter> parameters) {
	
	for (u64 i = 0u; i < parameters.size(); i++) {
		const Parameter& param = parameters[i];
		
		if (param.name.empty()) {
			LogWarning("tool '%' parameter #% has no name. Ignoring tool...", toolName, i); 
			return false;
		}
		
		if (param.type == Parameter::Type_String && param.defaultValue.stringValue.empty()) {
			LogWarning("tool '%' parameter '%': Default value is empty but parameter does not allow empty values. Ignoring tool...", toolName, param.name);
			return false;
		}
		
		if (param.type == Parameter::Type_Enum && param.enumValues.empty()) {
			LogWarning("tool '%' parameter '%': is an enum but has no enum values. Ignoring tool...", toolName, param.name);
			return false;
		}
		
		if (param.type == Parameter::Type_Enum && param.defaultValue.enumIndex >= param.enumValues.size()) {
			LogWarning("tool '%' parameter '%': default value (%) greater than enum size (%). Ignoring tool...",
				toolName, param.name, param.defaultValue.enumIndex, param.enumValues.size());
			return false;
		}
		
		if (param.type == Parameter::Type_Number) {
			if (param.defaultValue.numberValue < param.minValue) {
				LogWarning("tool '%' parameter '%': default value (%) is below the min value (%). Ignoring...", toolName, param.name, param.defaultValue.numberValue, param.minValue);
				return false;
			}
			
			if (param.defaultValue.numberValue > param.maxValue) {
				LogWarning("tool '%' parameter '%': default value (%) is above the max value (%). Ignoring...", toolName, param.name, param.defaultValue.numberValue, param.maxValue);
				return false;
			}
		}
	}
	
	return true;
}

static bool JsonToValue(const JsonTrace* trace, const cJSON* json, Tool* result);

bool Tool::LoadTools(const cJSON* json) {
		
	LogInfo("loading tools...");
	
	if (const cJSON* jsonTools = cJSON_GetObjectItem(json, "tools")) {
		const JsonTrace trace {nullptr, "tools"};
		
		if (!JsonToValue(&trace, jsonTools, &tools))
			return false;
			
		// check for validity
		for (u64 i = 0u; i < tools.size(); /**/) {
			const Tool& tool = tools[i];
			
			if (tool.name.empty()) {
				LogWarning("tool with no name found. Ignoring...");
				tools.erase(tools.begin() + i);
				continue;
			}
			if (tool.command.empty()) {
				LogWarning("tool '%' has no application. Ignoring...", tool.name); 
				tools.erase(tools.begin() + i);
				continue;
			}
			
			if (!CheckParameters(tool.name, tool.parameters)) {
				tools.erase(tools.begin() + i);
				continue;
			}
			
			 ++i;
		}
	}
	
	LogInfo("% valid tools defined", tools.size());
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Tool::GetDefaultValues(/*out*/ std::vector<ParameterValue>* parameterValues) const {
	parameterValues->clear();
	parameterValues->reserve(parameters.size());
	for (u64 i = 0u; i < parameters.size(); i++)
		parameterValues->push_back(parameters[i].defaultValue);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Tool::HasProgress() const {
	return !progress.regex.empty();
}

bool Tool::HasDiagnosticsMatcher() const {
	return !diagnosticsMatcher.regex.empty();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static JSON_TO_ENUM_BEGIN(Parameter::Type)
	JSON_TO_ENUM_MEMBER("none", Parameter::Type_None)
	JSON_TO_ENUM_MEMBER("string", Parameter::Type_String)
	JSON_TO_ENUM_MEMBER("enum", Parameter::Type_Enum)
	JSON_TO_ENUM_MEMBER("number", Parameter::Type_Number)
	JSON_TO_ENUM_MEMBER("bool", Parameter::Type_Bool)
JSON_TO_ENUM_END

static bool JsonToValue(const JsonTrace* trace, const cJSON* json, Parameter::EnumValue* result) {
	if (!JsonCheckType(trace, json, cJSON_String | cJSON_Object)) return false;
	 
	 if (cJSON_IsString(json)) {
		 result->name = cJSON_GetStringValue(json);
		 result->value = {};
	 } else {
	 	{
		 	const JsonTrace traceName {trace, "name"};
			const cJSON* jsonName = cJSON_GetObjectItem(json, "name");
			if (!jsonName) {
				JsonLogError(&traceName, "required property is missing");
				return false;
			}
			
			if (!JsonToValue(&traceName, jsonName, &result->name))
				return false;
		}
		{
			const JsonTrace traceName {trace, "value"};
			const cJSON* jsonName = cJSON_GetObjectItem(json, "value");
			if (!jsonName) {
				JsonLogError(&traceName, "required property is missing");
				return false;
			}
			
			if (!JsonToValue(&traceName, jsonName, &result->value))
				return false;
		}
	 }
 
	 return true;
 }
 

static JSON_TO_VALUE_BEGIN(Parameter)
	JSON_TO_VALUE_PROPERTY(type)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(enumValues)
	JSON_TO_VALUE_PROPERTY(allowEmpty)
	JSON_TO_VALUE_PROPERTY(maxValue)
	JSON_TO_VALUE_PROPERTY(minValue)
	JSON_TO_VALUE_PROPERTY(ifTrue)
	JSON_TO_VALUE_PROPERTY(ifFalse)
	
	if (auto nodeDefault = properties.extract("default"); !nodeDefault.empty()) {
		const JsonTrace trace {parentTrace, "default"};
		
		if (result->type == Parameter::Type_None) {
			// ignore
			
		} else if (result->type == Parameter::Type_String) {
			if (const char* str = cJSON_GetStringValue(nodeDefault.mapped())) {
				result->defaultValue.stringValue = str;
			} else {
				JsonLogWarning(&trace, "expected a [string]");
			}
			
		} else if (result->type == Parameter::Type_Number) {
			if (f64 number = cJSON_GetNumberValue(nodeDefault.mapped()); !isnan(number)) {
				result->defaultValue.numberValue = static_cast<s64>(number);
			} else {
				JsonLogWarning(&trace, "expected a [number]");
			}
		
		} else if (result->type == Parameter::Type_Enum) {
			if (const char* str = cJSON_GetStringValue(nodeDefault.mapped())) {
				
				u64 i = 0u;
				for (; i < result->enumValues.size(); i++)
					if (result->enumValues[i].name == str) goto found;
				
				JsonLogWarning(&trace, "'default' value must be in 'enumValues'.");
				i = U64_MAX;
				
			found:
				result->defaultValue.enumIndex = i;
			
			} else if (f64 idx = cJSON_GetNumberValue(nodeDefault.mapped()); !isnan(idx)) {
				result->defaultValue.enumIndex = static_cast<u64>(idx);
			
			} else {
				JsonLogWarning(&trace, "expected [number] - but was [%]", JsonTypeToString(json->type));
			}
		
		} else if (result->type == Parameter::Type_Bool) {
			if (cJSON_IsBool(nodeDefault.mapped())) {
				result->defaultValue.boolValue = cJSON_IsTrue(nodeDefault.mapped());
			} else {
				JsonLogWarning(&trace, "expected [bool] - but was [%]", JsonTypeToString(json->type));
			}
		
		} else {
			ASSERT_UNREACHABLE
		}
	}	
JSON_TO_VALUE_END

static JSON_TO_ENUM_BEGIN(Tool::Progress::Format)
	JSON_TO_ENUM_MEMBER("none", Tool::Progress::Format_None)
	JSON_TO_ENUM_MEMBER("percent", Tool::Progress::Format_Percent)
	JSON_TO_ENUM_MEMBER("absolute", Tool::Progress::Format_Absolute)
JSON_TO_ENUM_END

static JSON_TO_ENUM_BEGIN(Tool::ConsoleOpenFlags)
	JSON_TO_ENUM_MEMBER("never", Tool::ConsoleOpenFlags_Never)
	JSON_TO_ENUM_MEMBER("on-start", Tool::ConsoleOpenFlags_OnStart)
	JSON_TO_ENUM_MEMBER("on-exit-success", Tool::ConsoleOpenFlags_OnExitSuccess)
	JSON_TO_ENUM_MEMBER("on-exit-error", Tool::ConsoleOpenFlags_OnExitError)
	JSON_TO_ENUM_MEMBER("on-exit", Tool::ConsoleOpenFlags_OnExit)
	JSON_TO_ENUM_MEMBER("always", Tool::ConsoleOpenFlags_Always)
JSON_TO_ENUM_END

static JSON_TO_VALUE_BEGIN(Tool::Progress)
	JSON_TO_VALUE_PROPERTY(format)
	JSON_TO_VALUE_PROPERTY(regex)
	JSON_TO_VALUE_PROPERTY_NAMED("capture-group-value", captureGroupValue)
	JSON_TO_VALUE_PROPERTY_NAMED("capture-group-max", captureGroupMax)
	JSON_TO_VALUE_PROPERTY_NAMED("max", maxValue)
	JSON_TO_VALUE_PROPERTY_NAMED("hide-from-status-bar", hideFromStatusBar)
	JSON_TO_VALUE_CHECK_UNRECOGNIZED	
JSON_TO_VALUE_END

static JSON_TO_VALUE_BEGIN(Tool::DiagnosticsMatcher::ColorMapping)
	JSON_TO_VALUE_PROPERTY(key)
	//JSON_TO_VALUE_PROPERTY(color)
	JSON_TO_VALUE_CHECK_UNRECOGNIZED	
JSON_TO_VALUE_END

static JSON_TO_VALUE_BEGIN(Tool::DiagnosticsMatcher)
	JSON_TO_VALUE_PROPERTY(regex)
	JSON_TO_VALUE_PROPERTY_NAMED("capture-group-file", captureGroupFile)
	JSON_TO_VALUE_PROPERTY_NAMED("capture-group-line", captureGroupLine)
	JSON_TO_VALUE_PROPERTY_NAMED("capture-group-color", captureGroupLine)
	JSON_TO_VALUE_PROPERTY_NAMED("color-mapping", colorMapping)
	JSON_TO_VALUE_CHECK_UNRECOGNIZED	
JSON_TO_VALUE_END

static JSON_TO_VALUE_BEGIN(Tool)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(command)
	JSON_TO_VALUE_PROPERTY(description)
	JSON_TO_VALUE_PROPERTY(flags)
	JSON_TO_VALUE_PROPERTY(parameters)
	JSON_TO_VALUE_PROPERTY_NAMED("open-console", consoleOpenFlags)
	JSON_TO_VALUE_PROPERTY(progress)
	JSON_TO_VALUE_PROPERTY_NAMED("force-configuration", forceConfiguration)
	JSON_TO_VALUE_PROPERTY(external)
	JSON_TO_VALUE_PROPERTY_NAMED("diagnostics", diagnosticsMatcher)
	
	if (auto node = properties.extract("enviornment"); !node.empty()) {
		const JsonTrace trace {parentTrace, "enviornment"};
		
		std::unordered_map<std::string, std::string_view> enviornmentMap {};
		if (!JsonToValue<std::string_view>(&trace, node.mapped(), &enviornmentMap)) return false;
		
		for (auto it = enviornmentMap.begin(); it != enviornmentMap.end(); ++it) {
			result->environment.append(it->first);
			result->environment.push_back('=');
			result->environment.append(it->second);
			result->environment.push_back('\0');
		}
		result->environment.push_back('\0');
	}
		
	JSON_TO_VALUE_CHECK_UNRECOGNIZED	
JSON_TO_VALUE_END
