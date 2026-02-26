#include "basic-parser.hh"
#include "json/json-mapping.hh"
#include "json/json-mapping-stl.h"
#include "util/string-util.hh"

#include <algorithm>

using namespace BasicParser;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool MatchRule(const Rule* rule, std::span<Token> tokens, /*out*/ std::vector<Match>& matches, /*out*/ u64* matchedTokenCount) {

	if (tokens.empty())
		return false;

	switch (rule->type) {
		case Rule::Type_MatchTokenType: {

			const Token& token = tokens.front();
			if (token.type == rule->tokenType) {
				*matchedTokenCount = 1;

				if (!rule->label.empty()) {
					matches.emplace_back(Match {
						.label = rule->label,
						.startColumn = token.column,
						.endColumn   = token.EndColumn() });
				}
				return true;
			}
		} break;

		case Rule::Type_MatchText: {

			const Token& token = tokens.front();

			const auto funcStringCompare = rule->caseInsensitive ? StringCompare<true> : StringCompare<false>;
			for (const std::string& text : rule->texts) {
				
				const int cmpres = funcStringCompare(text, token.GetText());
				if (cmpres == 0) {
					*matchedTokenCount = 1;
					
					if (!rule->label.empty()) {
						matches.emplace_back(Match {
							.label = rule->label,
							.startColumn = token.column,
							.endColumn   = token.EndColumn() });
					}
					return true;
				
				} else if (cmpres > 0) {
					break;
				}
			}

		} break;

		case Rule::Type_MatchRegex: {

			const Token& token = tokens.front();
			if (rule->regex.Match(token.GetText(), nullptr)) {
				*matchedTokenCount = 1;
				
				if (!rule->label.empty()) {
					matches.emplace_back(Match {
						.label = rule->label,
						.startColumn = token.column,
						.endColumn   = token.EndColumn() });
				}
				return true;
			}
		} break;

		case Rule::Type_MatchInBetween: {

			ASSERT(rule->startingRule);
			const u64 sizeMatchesBefor = matches.size();

			Match* matchSlotFullRange = (!rule->label.empty())
				? &matches.emplace_back()
				: nullptr;

			u64 startingRuleTokenCount = 0u;
			if (!MatchRule(rule->startingRule.get(), tokens, matches, &startingRuleTokenCount)) {
				matches.resize(sizeMatchesBefor);
				return false;
			}

			if (rule->endingRule) {

				u64 endingRuleTokenCount = 0u;

				usize i = 0u;
				for (/**/; i < (tokens.size() - startingRuleTokenCount); i++) {

					const usize sizeBefor = matches.size();

					if (MatchRule(rule->endingRule.get(), tokens.subspan(startingRuleTokenCount + i), matches, &endingRuleTokenCount)) {
						goto matched_ending_rule;
					
					} else {
						ASSERT(matches.size() == sizeBefor);
						//matches.resize(sizeBefor);
					}

				}

				// did not match ending rule
				matches.resize(sizeMatchesBefor);
				return false;

			matched_ending_rule:

				const u64 totalTokensMatched = startingRuleTokenCount + i + endingRuleTokenCount;
				*matchedTokenCount = totalTokensMatched;

				if (matchSlotFullRange) {
					*matchSlotFullRange = Match {
						.label = rule->label,
						.startColumn = tokens[0].column,
						.endColumn   = tokens[totalTokensMatched - 1].EndColumn() };
				}

				return true;

			// no ending rule - match to end of string
			} else {

				*matchedTokenCount = tokens.size();

				if (matchSlotFullRange) {
					*matchSlotFullRange = Match {
						.label = rule->label,
						.startColumn = tokens.front().column,
						.endColumn   = tokens.back().EndColumn() };
				}

				return true;
			}
		} break;

		case Rule::Type_MatchSequence: {

			ASSERT(!rule->sequence.empty());

			if (tokens.size() < rule->sequence.size())
				return false;

			const usize sizeMatchesBefor = matches.size();

			Match* matchSlotFullRange =  (!rule->label.empty())
				? &matches.emplace_back()
				: nullptr;

			u64 totalTokensMatched = 0u;			
			for (const auto &sequenceRule : rule->sequence) {

				u64 numTokens = 0u;
				if (!MatchRule(&sequenceRule, tokens.subspan(totalTokensMatched), matches, &numTokens)) {

					if (sequenceRule.optional) {
						continue;
					} else {
						matches.resize(sizeMatchesBefor);
						return false;
					}
				}

				totalTokensMatched += numTokens;
			}

			*matchedTokenCount = totalTokensMatched;

			if (matchSlotFullRange) {
				*matchSlotFullRange = Match {
					.label = rule->label,
					.startColumn = tokens.front().column,
					.endColumn   = tokens[totalTokensMatched - 1].EndColumn() };
			}
			
			return true;
		} break;

		case Rule::Type_MatchOneOf: {
			
			const usize sizeMatchesBefor = matches.size();

			Match* matchSlotThisRule =  (!rule->label.empty())
				? &matches.emplace_back()
				: nullptr;

			for (const Rule& r : rule->rules) {

				u64 matchedTokens = 0u;
				if (MatchRule(&r, tokens, matches, &matchedTokens)) {
					
					*matchedTokenCount = matchedTokens;

					if (matchSlotThisRule) {
						*matchSlotThisRule = Match {
							.label = rule->label,
							.startColumn = tokens[0].column,
							.endColumn   = tokens[matchedTokens - 1].EndColumn() };
					}

					return true;
				}
			}

			// did not find a matching rule			
			matches.resize(sizeMatchesBefor);
			return false;

		} break;

		default: ASSERT_UNREACHABLE;
	}

	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void BasicParser::Tokenize(std::string_view text, /*out*/ std::vector<Token>* tokens) {

	tokens->clear();

	if (text.size() > U16_MAX)
		text = text.substr(0, U16_MAX);

	const char* textStart = text.data();
	while (!text.empty()) {

		// Alpha character? -> eat all following alphanumbneric chars
		if (IsAlpha(text.front())) {

			for (usize i = 1; i < text.size(); i++) {

				const char ch = text[i];
				if (!IsAlpha(ch) && !IsNumeric(ch)) {

					tokens->push_back(Token {
						.text = text.data(),
						.type = Token::Type_Word,
						.column = static_cast<u16>(text.data() - textStart),
						.length = static_cast<u16>(i) });

					text.remove_prefix(i);
					goto process_next_token;
				}
			}

			// end of string reached
			tokens->push_back(Token {
				.text = text.data(),
				.type = Token::Type_Word,
				.column = static_cast<u16>(text.data() - textStart),
				.length = static_cast<u16>(text.length()) });

			text = std::string_view {};

		// Numeric character? -> eat all following numbners
		} else if (IsNumeric(text.front())) {

			for (usize i = 1; i < text.size(); i++) {

				const char ch = text[i];
				if (!IsNumeric(ch)) {

					tokens->push_back(Token {
						.text = text.data(),
						.type = Token::Type_Number,
						.column = static_cast<u16>(text.data() - textStart),
						.length = static_cast<u16>(i) });

					text.remove_prefix(i);
					goto process_next_token;
				}
			}

			// end of string reached
			tokens->push_back(Token {
				.text = text.data(),
				.type = Token::Type_Number,
				.column = static_cast<u16>(text.data() - textStart),
				.length = static_cast<u16>(text.length()) });

			text = std::string_view {};

		// Whitespace? -> skip entirely
		} else if (IsWhitespace(text.front())) {
			text.remove_prefix(1);

		// must be symbol-character
		} else {
			tokens->push_back(Token {
				.text = text.data(),
				.type = Token::Type_Symbol,
				.column = static_cast<u16>(text.data() - textStart),
				.length = 1u });

			text.remove_prefix(1);
		}

	process_next_token:
		__noop;
	}
}

void BasicParser::MatchRules(std::span<Token> tokens, std::span<Rule> rules, /*out*/ std::vector<Match>* matches) {

	matches->clear();

	while (!tokens.empty()) {

		u64 matchedTokenCount = 0u;
		for (const Rule& rule : rules) {

			if (MatchRule(&rule, tokens, *matches, &matchedTokenCount)) {

				tokens = tokens.subspan(matchedTokenCount);
				goto process_remaining;
			}
		}

		tokens = tokens.subspan(1);

process_remaining:
		__noop;
	}
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<class ...Args>
static void InitRule(Rule* self)
{
	using enum BasicParser::Rule::Type;

	switch (self->type) {
		case Type_None:             break;
		case Type_MatchTokenType:   self->tokenType = Token::Type_Unknown; break;
		case Type_MatchText:      { new (&self->texts) std::vector<std::string>();
									self->caseInsensitive = false; } break;
		case Type_MatchRegex:       new (&self->regex) Regex(); break;
		case Type_MatchInBetween: { new (&self->startingRule) std::unique_ptr<Rule>();
								    new (&self->endingRule) std::unique_ptr<Rule>(); } break;
		case Type_MatchSequence:    new (&self->sequence) std::vector<Rule>(); break;
		case Type_MatchOneOf:       new (&self->rules) std::vector<Rule>(); break;
		default: ASSERT_UNREACHABLE;
	}
}

BasicParser::Rule::Rule(Rule::Type type /*= Type_None*/) noexcept
	: type(type)
	, optional(false)
	, label() {
	InitRule(this);
}

BasicParser::Rule::Rule(Rule&& other) noexcept
	: type(other.type)
	, optional(other.optional)
	, label(std::move(other.label)) {
	switch (other.type) {
		case Type_None:             break;
		case Type_MatchTokenType:   tokenType = other.tokenType; break;
		case Type_MatchText:      { new (&texts) std::vector<std::string>(std::move(other.texts));
									caseInsensitive = other.caseInsensitive; } break;
		case Type_MatchRegex:       new (&regex) Regex(std::move(other.regex)); break;
		case Type_MatchInBetween: { new (&startingRule) std::unique_ptr<Rule>(other.startingRule.release());
									new (&endingRule) std::unique_ptr<Rule>(other.endingRule.release()); } break;
		case Type_MatchSequence:    new (&sequence) std::vector<Rule>(std::move(other.sequence)); break;
		case Type_MatchOneOf:       new (&rules) std::vector<Rule>(std::move(other.rules)); break;
		default: ASSERT_UNREACHABLE;
	}
}

BasicParser::Rule::Rule(const Rule& other) noexcept
	: type(other.type)
	, optional(other.optional)
	, label(std::move(other.label)) {
	switch (other.type) {
		case Type_None:             break;
		case Type_MatchTokenType:   tokenType = other.tokenType; break;
		case Type_MatchText:      { new (&texts) std::vector<std::string>(other.texts);
									caseInsensitive = other.caseInsensitive; } break;
		case Type_MatchRegex:       new (&regex) Regex(other.regex); break;
		case Type_MatchInBetween: { new (&startingRule) std::unique_ptr<Rule>(new Rule(*other.startingRule));
									new (&endingRule) std::unique_ptr<Rule>(other.endingRule ? new Rule(*other.endingRule) : nullptr); } break;
		case Type_MatchSequence:    new (&sequence) std::vector<Rule>(other.sequence); break;
		case Type_MatchOneOf:       new (&rules) std::vector<Rule>(other.rules); break;
		default: ASSERT_UNREACHABLE;
	}
}

BasicParser::Rule::~Rule() noexcept {
	switch (type) {
		case Type_None:             break;
		case Type_MatchTokenType:   break;
		case Type_MatchText:        texts.~vector(); break;
		case Type_MatchRegex:       regex.~Regex(); break;
		case Type_MatchInBetween: { startingRule.~unique_ptr();
									endingRule.~unique_ptr(); } break;
		case Type_MatchSequence:    sequence.~vector(); break;
		case Type_MatchOneOf:       rules.~vector(); break;
		default: ASSERT_UNREACHABLE;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static JSON_TO_ENUM_BEGIN(BasicParser::Token::Type)
	JSON_TO_ENUM_MEMBER("unknown", Type_Unknown)
	JSON_TO_ENUM_MEMBER("word", Type_Word)
	JSON_TO_ENUM_MEMBER("number", Type_Number)
	JSON_TO_ENUM_MEMBER("symbol", Type_Symbol)
JSON_TO_ENUM_END

// @TODO rename matchType?
static JSON_TO_ENUM_BEGIN(BasicParser::Rule::Type)
	JSON_TO_ENUM_MEMBER("none", Type_None)
	JSON_TO_ENUM_MEMBER("matchTokenType", Type_MatchTokenType)
	JSON_TO_ENUM_MEMBER("matchText", Type_MatchText)
	JSON_TO_ENUM_MEMBER("matchRegex", Type_MatchRegex)
	JSON_TO_ENUM_MEMBER("matchInBetween", Type_MatchInBetween)
	JSON_TO_ENUM_MEMBER("matchSequence", Type_MatchSequence)
	JSON_TO_ENUM_MEMBER("matchOneOf", Type_MatchOneOf)
JSON_TO_ENUM_END

static bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, Regex* result) {

	std::string_view expression;
	if (!JsonToValue(parentTrace, json, &expression))
		return false;

	if (!result->Compile(expression)) {
		JsonLogError(parentTrace, "invalid regex '%'. %", expression, result->GetErrorString());
		return false;
	}

	return true;
}

JSON_TO_VALUE_BEGIN(BasicParser::Rule)
	JSON_TO_VALUE_PROPERTY_REQUIRED(type)
	JSON_TO_VALUE_PROPERTY(label)
	JSON_TO_VALUE_PROPERTY(optional)

	InitRule(result);

	switch (result->type) {
		case Rule::Type_None: break;

		case Rule::Type_MatchTokenType:
			JSON_TO_VALUE_PROPERTY(tokenType)
			break;

		case Rule::Type_MatchText: {
			JSON_TO_VALUE_PROPERTY(caseInsensitive)
			
			if (auto node = properties.extract("text"); !node.empty()) {
				const JsonTrace trace {parentTrace, "text"};
				
				if (!JsonToValue(&trace, node.mapped(), &result->texts.emplace_back()))
					return false;

			} else if (auto node = properties.extract("texts"); !node.empty()) {
				const JsonTrace trace {parentTrace, "texts"};
				
				if (!JsonToValue(&trace, node.mapped(), &result->texts))
					return false;

			} else {
				JsonLogError(parentTrace, "required property not found: 'text' or 'texts'");
				return false;
			}
			
			std::sort(result->texts.begin(), result->texts.end(),
				result->caseInsensitive
					? [] (const std::string& lhs, const std::string& rhs) { return StringCompare<true> (lhs, rhs) < 0; }
					: [] (const std::string& lhs, const std::string& rhs) { return StringCompare<false>(lhs, rhs) < 0; });
		} break;

		case Rule::Type_MatchRegex:
			JSON_TO_VALUE_PROPERTY_REQUIRED(regex)
			break;

		case Rule::Type_MatchInBetween: {
			JSON_TO_VALUE_PROPERTY_REQUIRED(startingRule)
			JSON_TO_VALUE_PROPERTY(endingRule)
		} break;

		case Rule::Type_MatchSequence: {
			JSON_TO_VALUE_PROPERTY_REQUIRED(sequence)
		} break;

		case Rule::Type_MatchOneOf: {
			JSON_TO_VALUE_PROPERTY_REQUIRED(rules)
		} break;

		default: {
			ASSERT_UNREACHABLE;
		}
	}
JSON_TO_VALUE_END
