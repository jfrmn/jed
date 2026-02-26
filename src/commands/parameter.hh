#pragma once
#include "basic.hh"

#include <string>
#include <vector>

struct ParameterValue {
	std::string stringValue = {};
	union {
		u64 enumIndex;
		s64 numberValue;
		bool boolValue;
	};
};
	
struct Parameter {

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

	Parameter::Type type = Parameter::Type_None;
	
	std::string name = {};
	
	ParameterValue defaultValue = {};
	
	std::vector<EnumValue> enumValues {};
	bool allowEmpty = true;
	s64 maxValue = S64_MAX;
	s64 minValue = S64_MIN;
	std::string ifTrue = {};
	std::string ifFalse = {};
};
