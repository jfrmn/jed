#pragma once
#include "basic.hh"

#include <string>
#include <string_view>
#include <span>
#include <cctype>


enum Encoding {
	 Encoding_Ascii = 0,
	 Encoding_Utf8,
	 Encoding_Utf16,
	 Encoding_Utf32,
	 Encoding_MAX
};

//---------------------------------------------------------
// Unicode functions
//---------------------------------------------------------

// returns true if this byte is part of a utf8 multibyte codepoint, but not the first byte
// returns false if this first byte of utf8 multibyte codepoint
// returns false for ascii/single-byte-character
bool IsMultibyteCodepointMember(char ch);

// convert to a different encoding
bool ToUtf32(std::string_view utf8,  /*out*/ std::span<u32>   utf32Buf, /*out*/ usize* len);
bool ToUtf16(std::string_view utf8,  /*out*/ std::span<wchar> utf16Buf, /*out*/ usize* len);
bool ToUtf8(std::wstring_view utf16, /*out*/ std::span<char>  utf8Buf,  /*out*/ usize* len);

//---------------------------------------------------------
// Char categorization
//---------------------------------------------------------

// returns ture for spaces, linebrakes and tabs otherwise false
bool IsWhitespace(char ch);
// returns true if char is in A-Z or a-z or underscore
bool IsAlpha(char ch);
// returns true if char is in 0-9
bool IsNumeric(char ch);
// returns true if char is numeric or alpha
bool IsAlphanumeric(char ch);

//---------------------------------------------------------
// StringCompare
//---------------------------------------------------------

// @DEPRICATED
// comapare 2 strings like strcmp
// with the option to perform this case insensitive
template <bool caseInsenstive>
int StringCompare(std::string_view lhs, std::string_view rhs);

// try to compare a utf8 string to a utf16 string
// attempts to compare them as theay are but of that's not possible
// it performs the necessary conversions
bool StringEquals(std::string_view lhs, std::wstring_view rhs);

bool StringEqualsCasesInsen(std::string_view lhs, std::string_view rhs);

//---------------------------------------------------------
// IterateLines
//---------------------------------------------------------
// iterate over all lines within a string
// respects \n, \r and \r\n
// in case of \r\n, lenLb will be 2 otherwise 1
//
// example for expectLinebreakOnLast = false:
//  the quick brown\n
//  fox jumps over\n
//  the lazy dog
// if expectLinebreakOnLast was true here, the 'the lazy dog' line gets ignored
//
// example for expectLinebreakOnLast = true:
//  the quick brown\n
//  fox jumps over\n
//  the lazy dog\n
// if expectLinebreakOnLast was false here, you'd get an additional empty line
//
// returns a pointer to the end of the last line that has been passed to func
//
const char* IterateLines(std::string_view string, void* userdata, void (*func)(void* userdata, const char* str, u64 strLen, u64 lenLb), bool expectLinebreakOnLast = false);

//---------------------------------------------------------
// Fuzzy Matching
//---------------------------------------------------------

struct FuzzyMatchResult {
	u64 position = 0u;
	u64 length = 0u;
	u64 matchedCount = 0u;
};

// fuzzy matches a string
// @TODO doucument this function
bool FuzzyMatch(std::string_view pattern, std::string_view fullText, /*out*/ FuzzyMatchResult* result);

int CompareFuzzyMatchResults(const FuzzyMatchResult& lhs, const FuzzyMatchResult& rhs);

//---------------------------------------------------------
// String formatting
//---------------------------------------------------------

std::string FormatString(const char* fmt, ...);
void FormatString(/*out*/ std::string* str, const char* fmt, ...);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template <bool caseInsenstive>
inline int StringCompare(std::string_view lhs, std::string_view rhs) {
	const size_t minSize = std::min(lhs.size(), rhs.size());

	for (size_t i = 0u; i < minSize; i++) {
		char chLhs, chRhs;
		if constexpr (caseInsenstive) {
			chLhs = std::tolower(lhs[i]);
			chRhs = std::tolower(rhs[i]);
		} else {
			chLhs = lhs[i];
			chRhs = rhs[i];
		}

		if (chLhs < chRhs) return -1;
		if (chLhs > chRhs) return  1;
	}

	if (lhs.size() < rhs.size()) return -1;
	if (lhs.size() > rhs.size()) return  1;
	return 0;
}
