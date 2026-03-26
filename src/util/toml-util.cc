#include "toml-util.hh"
#include "util/logging.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>


bool ExpectString(toml::node* node, std::string* strToSet) {
	if (toml::value<std::string>* val = node->as<std::string>()) {
		*strToSet = std::move(val->get());
		return true;
	} else {
		LogWarning("%: expected a string", node->source());
		return false;
	}
}

int ExpectString(toml::table* tbl, std::string_view key, std::string* strToSet, std::string_view fallback /*= {}*/) {
	toml::node* node = tbl->get(key);
	if (!node) {
		*strToSet = fallback;
		return -1;
	}
	
	toml::value<std::string>* val = node->as<std::string>();
	if (!val) {
		LogWarning("%: '%' expected a string", key, tbl->source());
		return 0;
	}
	
	*strToSet = std::move(val->get());
	return 1;
}

bool ExpectString(toml::array* arr, size_t index, std::string* strToSet, std::string_view fallback /*= {}*/) {
	toml::node* node = arr->get(index);
	ASSERT(node);
	
	toml::value<std::string>* val = node->as<std::string>();
	if (!val) {
		LogWarning("%: (index %) expected a string", index, arr->source());
		return false;
	}
	
	*strToSet = std::move(val->get());
	return true;
}

bool ExpectInteger(const toml::node* node, long long* outInt) {
	const toml::value<s64>* val = node->as_integer();
	if (!val) {
		LogWarning("%: expected an integer", node->source());
		return false;
	}
	
	*outInt = std::move(val->get());
	return true;	
}
