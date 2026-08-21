#include "parameter.hh"
#include "logging.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>


std::string_view Parameter::EnumValue::GetValue() const {
	return value.empty() ? name : value;
}

bool Parameter::FromToml(toml::node& node, Parameter* parameter) {
	toml::table* table = node.as_table();
	if (!table) {
		LogError("%s: expected an array", Str(table->source()));
		return false;
	}
	
	//
	// type
	//
	auto valType = table->get_as<std::string>("type");
	if (!valType) {
		LogError("%s: expected entry 'type' as string", Str(table->source()));
		return false;
	}
	
	if      (valType->get() == "none")   parameter->type = Parameter::Type_None;
	else if (valType->get() == "string") parameter->type = Parameter::Type_String;
	else if (valType->get() == "enum")   parameter->type = Parameter::Type_Enum;
	else if (valType->get() == "number") parameter->type = Parameter::Type_Number;
	else if (valType->get() == "bool")   parameter->type = Parameter::Type_Bool;
	else if (valType->get() == "path")   parameter->type = Parameter::Type_Path;
	else {
		LogError("%s: unknown parameter type: '%.*s'", Str(valType->source()), (int)valType->get().size(), valType->get().data());
		return false;
	}
	
	//
	// name
	//
	auto valName = table->get_as<std::string>("name");
	if (!valName) {
		LogError("%s: expected entry 'name' as string", Str(table->source()));
		return false;
	}
	parameter->name = std::move(valName->get());
	
	//
	// enum values
	//
	if (auto nodeEnumValues = table->get("values")) {
		if (auto arrEnumValues = nodeEnumValues->as_array()) {
			parameter->enumValues.reserve(arrEnumValues->size());
			
			for (toml::node& node : *arrEnumValues) {
				auto valValue = node.as_string();
				if (!valValue) continue;
				parameter->enumValues.push_back(EnumValue {
					.name = std::move(valValue->get())});
			}
		
		} else if (auto tblEnumValues = nodeEnumValues->as_table()) {
			parameter->enumValues.reserve(tblEnumValues->size());
			
			for (toml::impl::table_proxy_pair<false>& kvp : *tblEnumValues) {
				auto valValue = kvp.second.as_string();
				if (!valValue) continue;
				parameter->enumValues.push_back(EnumValue {
					.name = std::string {kvp.first},
					.value = std::move(valValue->get())});
			}
		} else {
			LogWarning("%s: expected an array or a table", Str(table->source()));
		}
	}
	
	//
	// other values
	//
	if (auto valEmpty = table->get_as<bool>("allow-empty")) {
		parameter->allowEmpty = valEmpty->get();
		if (parameter->type != Parameter::Type_String)
			LogWarning("%s: entry is only valid on string parameters", Str(valEmpty->source()));
	}
	
	if (auto valMax = table->get_as<s64>("max-value")) {
		parameter->maxValue = valMax->get();
		if (parameter->type != Parameter::Type_Number)
			LogWarning("%s: entry is only valid on number parameters", Str(valMax->source()));
	}
		
	if (auto valMin = table->get_as<s64>("min-value")) {
		parameter->minValue = valMin->get();
		if (parameter->type != Parameter::Type_Number)
			LogWarning("%s: entry is only valid on number parameters", Str(valMin->source()));
	}
	
	if (auto valIfTrue = table->get_as<std::string>("if-true")) {
		parameter->ifTrue = std::move(valIfTrue->get());
		parameter->hasIfTrue = true;
		if (parameter->type != Parameter::Type_Bool)
			LogWarning("%s: entry is only valid on boolean parameters", Str(valIfTrue->source()));
	}
		
	if (auto valIfFalse = table->get_as<std::string>("if-false")) {
		parameter->ifFalse = std::move(valIfFalse->get());
		parameter->hasIfFalse = true;
		if (parameter->type != Parameter::Type_Bool)
			LogWarning("%s: entry is only valid on boolean parameters", Str(valIfFalse->source()));
	}
		
	//
	// default value
	//
	if (auto nodeDefault = table->get("default")) {
		switch (parameter->type) {
			case Type_None: break;
			case Type_String: {
				auto valDefault = nodeDefault->as<std::string>();
				if (!valDefault) {
					LogWarning("%s: expected a string", Str(nodeDefault->source()));
					break;
				}
				parameter->defaultValue.stringValue = std::move(valDefault->get());
			} break;
			
			case Type_Enum: {
				if (auto valDefault = nodeDefault->as<std::string>()) {
					for (u64 i = 0u; i < parameter->enumValues.size(); i++) {
						if (parameter->enumValues[i].GetValue() == valDefault->get()) {
							parameter->defaultValue.enumIndex = i;
							goto found;
						}
					}
					LogWarning("%s: value must be in 'value'", Str(valDefault->source()));
				found: break;
				
				} else if (auto valDefault = nodeDefault->as<s64>()) {
					if (valDefault->get() < 0 || valDefault->get() >= static_cast<s64>(parameter->enumValues.size())) {
						LogWarning("%s: value is out of bounds", Str(valDefault->source()));
						break;
					}
					parameter->defaultValue.enumIndex = static_cast<u64>(valDefault->get());
				
				} else {
					LogWarning("%s: expected a number or a string", Str(nodeDefault->source()));
				}
			} break;
			
			case Type_Number: {
				auto valDefault = nodeDefault->as<s64>();
				if (!valDefault) {
					LogWarning("%s: expected a number", Str(valDefault->source()));
					break;
				}
				
				if (valDefault->get() < parameter->minValue) {
					LogWarning("%s: default value %d is below minimum %d", Str(valDefault->source()), valDefault->get(), parameter->minValue);
					break;
				}
				
				if (valDefault->get() > parameter->maxValue) {
					LogWarning("%s: default value %d is above maximim %d", Str(valDefault->source()), valDefault->get(), parameter->maxValue);
					break;
				}
			
				parameter->defaultValue.numberValue = valDefault->get();
			
			} break;
			
			case Type_Bool: {
				auto valDefault = nodeDefault->as<bool>();
				if (!valDefault) {
					LogWarning("%s: expected a bool", Str(valDefault->source()));
					break;
				}
				parameter->defaultValue.boolValue = valDefault->get();
			} break;
			
			// @TODO implement
			case Type_Path: break;
			
			default: break;
		}
	}
		
	return true;
}

void ParameterDefinition::GetDefaultValues(std::span<ParameterDefinition> parameters, std::vector<ParameterValue>* values) {
	values->reserve(parameters.size());
	for (const ParameterDefinition& p : parameters)
		values->push_back(p.defaultValue);
}
