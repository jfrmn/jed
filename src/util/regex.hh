#pragma once
#include "basic.hh"

#include <string_view>
#include <vector>

union RegexInternal;

// wrapper around stl regex, that hides exceptions and makes working with capture groups a bit easier
struct Regex {

	//---------------------------------------------------------
	// types

	struct MatchResult {

		struct Group {
			const char* begin = nullptr;
			const char* end = nullptr;
			
			std::string_view GetText() const;
			u64 Length() const;
		};

		std::vector<Group> groups = {};
			
		      Group& GetGroup(u64 index)       { return groups[index]; }
		const Group& GetGroup(u64 index) const { return groups[index]; }

		u64 Size() const { return groups.size(); }
	};

	//---------------------------------------------------------
	// functions
	
	bool Compile(std::string_view expression);
	void Reset();

	bool Match(std::string_view string, /*out*/ MatchResult* results) const;
	bool IsOk() const;
	std::string_view GetErrorString() const;

	//---------------------------------------------------------
	// construction & assignment

	Regex() noexcept;
	Regex(const Regex& other) noexcept;
	Regex(Regex&& other) noexcept;
	~Regex() noexcept;

private:
	RegexInternal* internal = nullptr;
	bool isOk = false;
};