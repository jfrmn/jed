#include "regex.hh"
#include "util/logging.hh"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

static constexpr u64 ERROR_BUFFER_SIZE = 128;
static char errorBuffer[ERROR_BUFFER_SIZE] {0};

static std::string_view GetErrorMessage(int errorNumber) {
	memset(errorBuffer, 0, sizeof(char) * ERROR_BUFFER_SIZE);
	const u64 len = pcre2_get_error_message(errorNumber, reinterpret_cast<unsigned char*>(errorBuffer), ERROR_BUFFER_SIZE);	
	return std::string_view {errorBuffer, len};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

std::string_view RegexMatch::Group::GetText() const {
	return std::string_view {begin, end};
}

u64 RegexMatch::Group::Length() const {
	return end - begin;
}

void RegexMatch::Reserve(u32 captureGroupCount) {
	const u32 newCapacity = captureGroupCount + 1u;
	
	if (this->capacity >= newCapacity) return;
	
	pcre2_match_data_free(data);
	this->data = pcre2_match_data_create(newCapacity, nullptr);
	this->capacity = newCapacity;
}

void RegexMatch::ClearSubject() {
	subject = {};
	offset = 0;
	groupCount = 0;
}

RegexMatch::Group RegexMatch::GetFullMatch() const {
	return GetGroup(0);
}

RegexMatch::Group RegexMatch::GetGroup(u32 index) const {
	u64* ovector = pcre2_get_ovector_pointer(data);
	const u64 offsetBegin = ovector[(index*2)];
	const u64 offsetEnd   = ovector[(index*2)+1];	
	
	ASSERT(offsetBegin <  subject.size());
	ASSERT(offsetEnd   <= subject.size());
	ASSERT(offsetBegin <= offsetEnd);
	
	return Group {
		.begin = subject.data() + offsetBegin,
		.end   = subject.data() + offsetEnd};
}

std::string_view RegexMatch::GetGroupText(u32 index) const {
	const Group group = GetGroup(index);
	return group.GetText();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool Regex::Compile(std::string_view expression, /*out*/ RegexError* error) {
	Reset();
			
	PCRE2_SIZE errorOffset = 0u;
	int errorNum = 0;
	// @TODO optimize by using a compile context for multiple regex compilations
	code = pcre2_compile(
		reinterpret_cast<const unsigned char*>(expression.data()), expression.size(), 0, &errorNum, &errorOffset, NULL);
		
	if (!code) {
		if (error) {
			error->code = errorNum;
			
			error->message.resize(128u);
			const int messageSize = pcre2_get_error_message(errorNum, reinterpret_cast<unsigned char*>(error->message.data()), error->message.size());
			if (messageSize >= 0) {
				error->message.resize(messageSize);							
				
			} else {
				LogError("error getting pcre2 error message: % (%)", messageSize, 
					messageSize == PCRE2_ERROR_BADDATA
						? "PCRE2_ERROR_BADDATA"
					: messageSize == PCRE2_ERROR_NOMEMORY
						? "PCRE2_ERROR_NOMEMORY"
						: "?");

				const u64 len = strnlen_s(error->message.data(), 128);
				error->message.resize(len);
			}
			
			error->position = errorOffset;
		}
		
		return false;
	}
	
	errorNum = pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &captureGroupCount);
	if (errorNum < 0) {
		LogError("pcre2_pattern_info() failed. % %", errorNum, GetErrorMessage(errorNum));
		return false;
	}
	isOk = true;
	
	errorNum = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
	if (errorNum < 0) {
		ASSERT(errorNum != PCRE2_ERROR_BADOPTION);
		LogWarning("pcre2_jit_compile() failed. % %", errorNum, GetErrorMessage(errorNum));
	}
	isJitCompiled = true;
	
	return true;
}

void Regex::Reset() {
	if (code) {
		pcre2_code_free(code);
		code = nullptr;
	}
	captureGroupCount = 0u;
	isOk = false;
	isJitCompiled = false;
}

bool Regex::Match(std::string_view subject, /*out*/ RegexMatch* match) const {
	
	if (!isOk) return false;
	ASSERT(code);
	
	if (match->subject.empty()) {
		
		match->Reserve(captureGroupCount);
		
		match->subject = subject;
		match->offset = 0;
		match->groupCount = captureGroupCount + 1;
		
	} else {
		ASSERT(subject == match->subject);
		ASSERT(captureGroupCount + 1 <= match->capacity);
		
		u32 options = 0;
		if (!pcre2_next_match(match->data, &match->offset, &options))
			return false;
	}
	
	u64* ovector = pcre2_get_ovector_pointer(match->data);
	for (u32 i = 0; i < pcre2_get_ovector_count(match->data) * 2; i++)
		ovector[i] = PCRE2_UNSET;
	
	auto pcre2_match_func = isJitCompiled ? pcre2_jit_match : pcre2_match;
	const int result = pcre2_match_func(code, reinterpret_cast<const unsigned char*>(subject.data()), subject.size(), match->offset, 0, match->data, nullptr);
		
	if (result >= 0) {
		return true;
	} else if (result == PCRE2_ERROR_NOMATCH) {
		return false;
	} else {
		LogError("regex pcre2_(jit)_match failed: % %", result, GetErrorMessage(result));
		return false;
	}
}

u64 Regex::GetCaptureGroupByName(const char* name) const {
	const int result = pcre2_substring_number_from_name(code, reinterpret_cast<const unsigned char*>(name));
	if (result == PCRE2_ERROR_NOSUBSTRING) return U64_MAX;
	if (result < 0) {
		LogWarning("pcre2_substring_number_from_name failed. % %", result, GetErrorMessage(result));
		return U64_MAX;
	}
	
	return static_cast<u64>(result); 
}

Regex::Regex(const Regex& other) noexcept 
	: code(other.code ? pcre2_code_copy(other.code) : nullptr)
	, captureGroupCount(other.captureGroupCount)
	, isOk(other.isOk)
	, isJitCompiled(other.isJitCompiled) {}
	
Regex::Regex(Regex&& other) noexcept
	: code(other.code)
	, captureGroupCount(other.captureGroupCount)
	, isOk(other.isOk)
	, isJitCompiled(other.isJitCompiled) {
	other.code = nullptr;
	other.captureGroupCount = 0u;
	other.isOk = false;
	other.isJitCompiled = false;
}
	
Regex::~Regex() noexcept {
	pcre2_code_free(code);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

FormatArgument F(const RegexError& error) {
	return FormatArgument {
		.userdata = &error,
		.Write = [] (const void* userdata, std::ostream* sink) {
			auto error = static_cast<const RegexError*>(userdata);
			*sink << error->code << ": " << error->message << " at pos " << error->position;
		}
	};
}

