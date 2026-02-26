#include "regex.hh"

#include <regex>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
union RegexInternal { 	
	std::regex regex;
	std::regex_error error;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Regex::Compile(std::string_view expression) {
	
	Reset();
	
	try {
		internal = static_cast<RegexInternal*>(malloc(sizeof(RegexInternal)));
		new (&internal->regex) std::regex(expression.begin(), expression.end(), std::regex::ECMAScript | std::regex::optimize);
		isOk = true;
		return true;
	
	} catch (std::regex_error& e) {
	
		internal->regex.~basic_regex();
		new (&internal->error) std::regex_error(std::move(e));
		isOk = false;
		return false;
	}
}

void Regex::Reset() {
	if (internal) {
		if (isOk) internal->regex.~basic_regex();
		else internal->error.~regex_error();
		
		free(internal);
		internal = nullptr;
	}
	
	isOk = false;
}

bool Regex::IsOk() const {
	return isOk;
}

std::string_view Regex::GetErrorString() const {
	
	if (isOk)
		return "ok";

	if (!internal)
		return "regex not compiled yet";
	
	const char* what = internal->error.what();
	return std::string_view(what);
}

bool Regex::Match(std::string_view string, /*out*/ MatchResult* results) const {
	
	if (!isOk)
		return false;

	std::match_results<std::string_view::iterator> matchResults {};
	if (!std::regex_search(string.begin(), string.end(), matchResults, internal->regex))
		return false;

	if (results) {
		results->groups.reserve(matchResults.size());

		for (const auto& subMatch : matchResults) {
			MatchResult::Group& group = results->groups.emplace_back();

			group.begin = string.data() + std::distance(string.begin(), subMatch.first);
			group.end   = string.data() + std::distance(string.begin(), subMatch.second);
		}
	}

	return true;	
}

std::string_view Regex::MatchResult::Group::GetText() const {
	return std::string_view(begin, end);
}

u64 Regex::MatchResult::Group::Length() const {
	ASSERT(end <= begin);
	return end - begin;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Regex::Regex() noexcept {}
Regex::Regex(const Regex& other) noexcept {
	isOk = other.isOk;
	if (other.internal) {
		internal = static_cast<RegexInternal*>(malloc(sizeof(RegexInternal)));
		
		if (isOk) new (&internal->regex) std::regex(other.internal->regex);
		else new (&internal->error) std::regex_error(other.internal->error);
	}
}

Regex::Regex(Regex&& other) noexcept
	: isOk(other.isOk)
	, internal(other.internal) {
	other.isOk = false;
	other.internal = nullptr;
}

Regex::~Regex() noexcept {
	Reset();
}
