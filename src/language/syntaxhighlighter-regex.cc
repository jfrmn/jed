#include "syntaxhighlighter-regex.hh"
#include "settings.hh"

#include "graphics/glyph-run.hh"
#include "graphics/effects.hh"

#include "editor/editor.hh"
#include "util/logging.hh"
#include "text/text-buffer.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>

bool SyntaxHighlighterRegex::FromToml(toml::node* toml) {
	
	toml::array* arrRules = toml->as_array();	
	if (!arrRules) {
		LogError("%: expected an array", F(toml->source()));
		return false;
	}
	
	rules.clear();
	rules.reserve(arrRules->size());

	for (toml::node& nodeRule : *arrRules) {
		toml::table* table = nodeRule.as_table();
		if (!table) {
			LogWarning("%: expected a table", F(nodeRule.source()));
			continue;
		}
		
		const toml::value<std::string>* regex = table->get_as<std::string>("regex");
		if (!regex) {
			LogWarning("%: 'regex' is missing", table->source());
			continue;
		}
		
		Rule rule {};
		if (RegexError error; !rule.regex.Compile(regex->get(), &error)) {
			LogWarning("%: regex did not compile: %. Ignoring rule...", F(regex->source()), error.message);
			continue;
		}
		
		if (toml::value<std::string>* nodeLabel = table->get_as<std::string>("label")) {
			rule.labels.push_back(std::move(nodeLabel->get()));
		}
		
		if (toml::array* arrLabels = table->get_as<toml::array>("labels")) {
			rule.labels.reserve(arrLabels->size());
				
			for (toml::node& node : *arrLabels) {
				auto value = node.as_string();
				if (!value) {
					LogWarning("%: expected a string", F(node.source()));
					continue;
				}
				
				rule.labels.push_back(std::move(value->get()));
			}
		}
		
		rules.push_back(std::move(rule));
	}
	
	u32 maxCaptureGroupCount = 0;
	for (const Rule& rule : rules) {
		if (maxCaptureGroupCount < rule.regex.captureGroupCount)
			maxCaptureGroupCount = rule.regex.captureGroupCount;
	}
	match.Prepare(maxCaptureGroupCount + 1u);
	
	return true;
}

// @DUMMY
static D2D1_COLOR_F GetColorForLabel(std::string_view label) {
	if (label == "keyword")
		return D2D1::ColorF(D2D1::ColorF::RoyalBlue);
	else if (label == "function")
		return D2D1::ColorF(D2D1::ColorF::LemonChiffon);
	else if (label == "control-flow")
		return D2D1::ColorF(D2D1::ColorF::RoyalBlue);
	else if (label == "string")
		return D2D1::ColorF(D2D1::ColorF::LightSalmon);
	else if (label == "comment")
		return D2D1::ColorF(D2D1::ColorF::LightGray);
	else if (label == "type")
		return D2D1::ColorF(D2D1::ColorF::DarkTurquoise);
	else if (label == "preprocessor")
		return D2D1::ColorF(D2D1::ColorF::HotPink);
	else if (label == "number")
		return D2D1::ColorF(D2D1::ColorF::LimeGreen);
	else if (label == "tag")
		return D2D1::ColorF(D2D1::ColorF::Gold);
	else
		return D2D1::ColorF(D2D1::ColorF::White);
}

void SyntaxHighlighterRegex::Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) {
	for (u64 ln = fromLine; ln <= toLine; ln++) {
		const TextBuffer::Line& line = editor->textController.buffer.GetLineAt(ln);
		const GlyphRun& run = editor->glyphRuns[ln];
		
		for (const Rule& rule : rules) {
			match.ClearSubject();
			
			while(rule.regex.Match(line.GetText(), &match)) {
				for (u64 i = 0u; i < std::min<u64>(rule.labels.size(), match.groupCount); i++) {
					if (rule.labels[i].empty()) continue;
					const RegexMatch::Group& group = match.GetGroup(static_cast<u32>(i));
					
					const D2D_COLOR_F color = GetColorForLabel(rule.labels[i]);
					brush->SetColor(color);
					
					const u64 start = group.begin - line.data;
					const u64 end   = group.end   - line.data;
					
					f32 offsetStart, offsetEnd;
					run.MeasureOffsetRange(start, end, &offsetStart, &offsetEnd);
					
					renderTarget->FillRectangle(
						D2D_RECT_F {
							.left   = offsetStart,
							.top    = (settings.fontEditor.lineHeight * ln),
							.right  = offsetEnd,
							.bottom = (settings.fontEditor.lineHeight * (ln+1)) },
						brush);
				}
			}
		}
	}
}
