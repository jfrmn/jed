#include "string-util.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define MULTIBYTE_INDICATOR_2_BYTES 0b1100'0000
#define MULTIBYTE_INDICATOR_3_BYTES 0b1110'0000
#define MULTIBYTE_INDICATOR_4_BYTES 0b1111'0000

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool IsMultibyteCodepointMember(char ch) {
	return (ch & 0b1100'0000) == 0b1000'0000;
}

bool IsWhitespace(char ch) {
	return (ch == ' ')
		|| (ch == '\r')
		|| (ch == '\n')
		|| (ch == '\t')
		|| (ch == '\v');
}

bool IsAlpha(char ch) {
	return (ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch == '_');
}

bool IsNumeric(char ch) {
	return (ch >= '0' && ch <= '9');
}

bool IsAlphanumeric(char ch) {
	return IsAlpha(ch) || IsNumeric(ch);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ToUtf32(std::string_view utf8, /*out*/ std::span<u32> utf32Buf, /*out*/ usize* len) {

	if (utf32Buf.empty()) {
	
		u64 requiredSize = 0u;
		for (usize i = 0; i < utf8.size(); /*noop*/) {
		
			requiredSize += 1;
			const u32 ch = static_cast<u32>(utf8[i]) & 0xFF;
			
			if (ch < MULTIBYTE_INDICATOR_2_BYTES) i += 1;
			else if (ch < MULTIBYTE_INDICATOR_3_BYTES) i += 2;
			else if (ch < MULTIBYTE_INDICATOR_4_BYTES) i += 3;
			else i += 4;
		}
		
		ASSERT(len);
		*len = requiredSize;
	
	} else {
		
		u64 currentLen = 0u;
		for (usize i = 0; i < utf8.size(); /*noop*/) {
			
			const u32 ch = static_cast<u32>(utf8[i]) & 0xFF;
			
			if (ch < MULTIBYTE_INDICATOR_2_BYTES) {
				utf32Buf[currentLen++] = ch;
				i += 1;
				
			} else if (ch < MULTIBYTE_INDICATOR_3_BYTES) {
				utf32Buf[currentLen++] = 
					((utf8[i  ] & 0x1F) << 6) |
				 	(utf8[i+1] & 0x3F);
				i += 2;
				
			} else if (ch < MULTIBYTE_INDICATOR_4_BYTES) {
				utf32Buf[currentLen++] = 
					((utf8[i  ] & 0x0F) << 12) |
					((utf8[i+1] & 0x3F) <<  6) |
				 	(utf8[i+2] & 0x3F);
				i += 3;
				
			} else {
				utf32Buf[currentLen++] = 
					((utf8[i  ] & 0x07) << 18) |
					((utf8[i+1] & 0x3F) << 12) |
					((utf8[i+2] & 0x3F) <<  6) |
				 	(utf8[i+3] & 0x3F);
				i += 4;
			}
			
			// @TODO clarify behavior: should be have like the windows MultiByteToWideChar functions
			if (currentLen > utf32Buf.size())
				return false;
		}
		
		if (len)
		   *len = currentLen;
	}
	
	return true;
}

bool ToUtf16(std::string_view utf8, /*out*/ std::span<wchar> utf16Buf, /*out*/ usize* len) {
	
	if (utf16Buf.empty()) {
	
		const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), NULL, 0);
		if (requiredSize == 0)
			return false;

		ASSERT(len);
		*len = requiredSize;
		return true;
	
	} else {
			
		const int result = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), utf16Buf.data(), static_cast<int>(utf16Buf.size()));
		if (result == 0)
			return false;
		
		if (len)
		   *len = result;
			
		return true;
	}
}

bool ToUtf8(std::wstring_view utf16, /*out*/ std::span<char> utf8Buf, /*out*/ usize* len) {
	
	if (utf8Buf.empty()) {
	
		const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), NULL, 0, NULL, NULL);
		if (requiredSize == 0)
			return false;

		ASSERT(len);
		*len = requiredSize;
		return true;
	
	} else {
			
		const int result = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), utf8Buf.data(), static_cast<int>(utf8Buf.size()), NULL, NULL);
		if (result == 0)
			return false;
		
		if (len)
           *len = result;
			
		return true;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StringEqualsCasesInsen(std::string_view lhs, std::string_view rhs) {
	if (lhs.size() != rhs.size()) return false;

	for (size_t i = 0u; i < lhs.size(); i++) {
		if (std::tolower(lhs[i]) != std::tolower(rhs[i]))
			return false;
	}

	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool StringEquals(std::string_view lhs, std::wstring_view rhs) {
	if (lhs.size() != rhs.size())
		return false;
		
	const u64 size = lhs.size();
	for (u64 i = 0u; i < size; i++) {
		const  char  ch = lhs[i];
		const wchar wch = rhs[i];
			
		if ((ch & 0x7F) != ch || (wch & 0x7F) != wch)
			goto conversion;
			
		if (ch != static_cast<char>(wch))
			return false;
	}
		
	return true;
	
conversion:
	u64 requiredSize = 0;
	if (!ToUtf8(rhs, {}, &requiredSize)) return false;
	
	auto narrowRhs = new char[requiredSize];
	DEFER(delete[] narrowRhs);
	if (!ToUtf8(rhs, std::span<char>(narrowRhs, requiredSize), nullptr)) return false;
	
	return lhs == narrowRhs;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const char* IterateLines(std::string_view string, void *userdata, void (*func)(void *userdata, const char *str, u64 strLen, u64 lenLb), bool expectLinebreakOnLast) {
	ASSERT(func);
	
	const char* end       = string.data() + string.size();
	const char* linestart = string.data();
	const char* lineend   = nullptr;
	const char* it        = string.data();
	
	while (it != end) {

		if (*it == '\n') {
			
			lineend = it;
			func(userdata, linestart, (it - linestart), 1u);

			it++;
			linestart = it;
		
		} else if (*it == '\r') {
			
			const auto itAfterCR = it + 1;
			if (itAfterCR < end && *itAfterCR == '\n') {

				lineend = it;
				func(userdata, linestart, (it - linestart), 2u);
				
				it += 2;
				linestart = it;
			
			} else {
			
				lineend = it;
				func(userdata, linestart, (it - linestart), 1u);

				it++;
				linestart = it;
			}

		} else {
			it++;
		}
	}

	if (!expectLinebreakOnLast) {
		lineend = it;
		func(userdata, linestart, (end - linestart), 0u);
	}
	
	return lineend;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool FuzzyMatch(std::string_view pattern, std::string_view fullText, /*out*/ FuzzyMatchResult* result) {
	ASSERT(!fullText.empty())
	ASSERT(result)

	if (fullText.size() < pattern.size())
		return false;
	
	ASSERT(!pattern.empty())
	for (u64 i = 0; i <= fullText.size() - pattern.size(); i++) {
		
		u64 currentMatchedChars = 0u;		
		for (u64 j = 0; j < pattern.size(); j++) {
			
			// getting rid of C4334
			constexpr u64 ONE = 1u;
			
			if (fullText[i + j] == pattern[j])
				currentMatchedChars += ONE;
		}
		
		if (result->matchedCount < currentMatchedChars) {
			result->matchedCount = currentMatchedChars;
			result->position = i;
			result->length = pattern.size();
		}
	}
	
	return (result->matchedCount > 0);
}

int CompareFuzzyMatchResults(const FuzzyMatchResult& lhs, const FuzzyMatchResult& rhs) {
	if (lhs.matchedCount > rhs.matchedCount) return  1;
	if (lhs.matchedCount < rhs.matchedCount) return -1;
			
	if (lhs.position < rhs.position) return  1;
	if (lhs.position > rhs.position) return -1;
	
	if (lhs.length < rhs.length) return  1;
	if (lhs.length > rhs.length) return -1;
			
	return 0;
}
