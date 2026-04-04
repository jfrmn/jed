#pragma once
#include "basic.hh"

#include <string>

struct Regex;
struct FormatArgument;
struct pcre2_real_code_8;
struct pcre2_real_match_data_8;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct RegexError {
	int code = 0u;
	std::string message = {};
	u64 position = 0;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct RegexMatch {
	
	//------------------------------------------
	// types
	//------------------------------------------	

	struct Group {
		const char* begin = nullptr;
		const char* end = nullptr;
		
		std::string_view GetText() const;
		u64 Length() const;
	};
	
	//------------------------------------------
	// data
	//------------------------------------------	

	std::string_view subject = {};
	pcre2_real_match_data_8* data = nullptr;
	u64 offset = 0u; // for multiple matches
	u32 capacity = 0u;
	u32 groupCount = 0u;

	//------------------------------------------	
	// functions
	//------------------------------------------	
	
	void Reserve(u32 captrueGroupCount);
	void ClearSubject();
	
	Group GetFullMatch() const;	
	Group GetGroup(u32 index) const;
	std::string_view GetGroupText(u32 index) const;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct Regex {
	
	//------------------------------------------
	// data
	//------------------------------------------	
	
	pcre2_real_code_8* code = nullptr;
	u32 captureGroupCount = 0u; // these are ADDITIONAL groups (group at 0 is always the full match)
	bool isOk = false;
	bool isJitCompiled = false;

	//------------------------------------------	
	// functions
	//------------------------------------------	
		
	bool Compile(std::string_view expression, /*out*/ RegexError* error);
	void Reset();

	bool Match(std::string_view subject, RegexMatch* match) const;
	u64 GetCaptureGroupByName(const char* name) const; // needs to be zero terminated

	//------------------------------------------	
	// construction	
	//------------------------------------------	
	
	Regex() noexcept = default;
	Regex(const Regex& other) noexcept;
	Regex(Regex&& other) noexcept;
	~Regex() noexcept;
};

FormatArgument F(const RegexError& error);

