#include "parameter.hh"

std::string_view Parameter::EnumValue::GetValue() const {
	return value.empty() ? name : value;
}
