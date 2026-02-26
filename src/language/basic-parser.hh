#pragma once
#include "basic.hh"
#include "util/regex.hh"

#include <string>
#include <vector>
#include <memory>
#include <span>

namespace BasicParser {

	//-----------------------------------------------------------------------------
	// Token
	//-----------------------------------------------------------------------------
	
	struct Token {
		enum Type : u32 {
			Type_Unknown = 0,
			Type_Word,
			Type_Number,
			Type_Symbol
		};

		const char* text = nullptr;
		Type type  = Type_Unknown;
		u16 column = 0u;
		u16 length = 0u;

		std::string_view GetText() const { return std::string_view {text, length}; }
		u16 EndColumn() const { return static_cast<u16>(column + length); }
	};

	//-----------------------------------------------------------------------------
	// Match
	//-----------------------------------------------------------------------------
	
	struct Match {
		std::string_view label = {};
		u16 startColumn = 0u;
		u16 endColumn = 0u;
	};

	//-----------------------------------------------------------------------------
	// Rule
	//-----------------------------------------------------------------------------

	struct Rule {

		//------------------------------------------------
		// types

		enum Type {
			Type_None = 0,
			Type_MatchTokenType,
			Type_MatchText,
			Type_MatchRegex,
			Type_MatchInBetween,
			Type_MatchSequence,
			Type_MatchOneOf
		};

		Type type = Type_None;
		bool optional = false; // only meaningfull when inside a sequence
		std::string label = {};

		union {
			struct {} nodata;
			Token::Type tokenType;
			struct {
				std::vector<std::string> texts;
				bool caseInsensitive; };
			Regex regex;
			struct {
				std::unique_ptr<Rule> startingRule;
				std::unique_ptr<Rule> endingRule; };
			std::vector<Rule> sequence;
			std::vector<Rule> rules;
		};

		//------------------------------------------------
		// construction

		explicit Rule(Rule::Type type = Type_None) noexcept;
		explicit Rule(Rule&& other) noexcept;
		explicit Rule(const Rule& other) noexcept;
		~Rule() noexcept;
	};

	void Tokenize(std::string_view text, /*out*/ std::vector<Token>* tokens);
	void MatchRules(std::span<Token> tokens, std::span<Rule> rules, /*out*/ std::vector<Match>* matches);

}

struct cJSON;
struct JsonTrace;

bool JsonToValue(const JsonTrace* trace, const cJSON* json, BasicParser::Rule* result);
