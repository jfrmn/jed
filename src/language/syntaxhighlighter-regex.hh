#pragma once
#include "basic.hh"
#include "language/syntaxhighlighter.h"
#include "util/regex.hh"

#include <string>
#include <vector>

namespace toml { class node; }

struct SyntaxHighlighterRegex : public SyntaxHighlighter {
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
	
	struct Rule {
		Regex regex = {};
		std::vector<std::string> labels = {};
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	std::vector<Rule> rules = {};
	RegexMatch match = {};
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	bool FromToml(toml::node* toml);
	
	virtual void Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) override;
};
