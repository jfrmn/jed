#pragma once
#include <string>

namespace toml {
	class node;
	class table;
	class array;
}

bool ExpectString(toml::node* node, std::string* strToSet);
int  ExpectString(toml::table* tbl, std::string_view key, std::string* strToSet, std::string_view fallback = {});
bool ExpectInteger(const toml::node* node, long long* outInt);
