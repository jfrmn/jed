#pragma once
#include "basic.hh"

#include <string>
#include <vector>
#include <span>

namespace toml { class node; }

// NOTE:
// We use these for tools and commands and maybe in future for the settings ui

// @TODO:
// - in the value use a std::string_view + unique_ptr. This saves us one u64
// - add an unsigned int, we use them heavily in the commands
// - use optionals for ifTrue/ifFalse

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Parameter value
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct ParameterValue {
	std::string stringValue = {};
	union {
		u64 enumIndex;
		s64 numberValue;
		bool boolValue;
	};
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Parameter definition
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct ParameterDefinition {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	enum Type {
		 Type_None = 0,
	 	 Type_String,
	 	 Type_Enum,
	 	 Type_Number,
	 	 Type_Bool,
	 	 Type_Path // @TODO not implemented
	};
	
	struct EnumValue {
		std::string name = {};
		std::string value = {};
		std::string_view GetValue() const;
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	ParameterDefinition::Type type = ParameterDefinition::Type_None;
	
	std::string name = {};
	
	ParameterValue defaultValue = {};
	
	std::vector<EnumValue> enumValues {};
	bool allowEmpty = true;
	s64 maxValue = S64_MAX;
	s64 minValue = S64_MIN;
	std::string ifTrue = {};
	std::string ifFalse = {};
	bool hasIfTrue = false;
	bool hasIfFalse = false;	
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	static bool FromToml(toml::node& node, /*out*/ ParameterDefinition* parameter);
	static void GetDefaultValues(std::span<ParameterDefinition> parameter, std::vector<ParameterValue>* values);
};

using Parameter = ParameterDefinition;
