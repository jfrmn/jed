#include "syntaxhighlighter-regex.hh"
#include "settings.hh"

#include "glyph-run.hh"
#include "graphics.hh"

#include "editor/editor.hh"
#include "logging.hh"
#include "text/text-buffer.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>

bool SyntaxHighlighterRegex::FromToml(toml::node* toml) {
	
	toml::array* arrRules = toml->as_array();	
	if (!arrRules) {
		LogError("%s: expected an array", Str(toml->source()));
		return false;
	}
	
	rules.clear();
	rules.reserve(arrRules->size());

	for (toml::node& nodeRule : *arrRules) {
		toml::table* table = nodeRule.as_table();
		if (!table) {
			LogWarning("%s: expected a table", Str(nodeRule.source()));
			continue;
		}
		
		const toml::value<std::string>* regex = table->get_as<std::string>("regex");
		if (!regex) {
			LogWarning("%s: 'regex' is missing", Str(table->source()));
			continue;
		}
		
		Rule rule {};
		if (RegexError error; !rule.regex.Compile(regex->get(), &error)) {
			LogWarning("%s: regex did not compile: %s. Ignoring rule...", Str(regex->source()), error.message.c_str());
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
					LogWarning("%s: expected a string", Str(node.source()));
					continue;
				}
				
				rule.labels.push_back(std::move(value->get()));
			}
		}
		
		rules.push_back(std::move(rule));
	}
	
	u32 maxCaptureGroupCount = 0;
	for (const Rule& rule : rules) {
		if (maxCaptureGroupCount < rule.regex.additionalCaptureGroupCount)
			maxCaptureGroupCount = rule.regex.additionalCaptureGroupCount;
	}
	match.Reserve(maxCaptureGroupCount);
	
	return true;
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
					
					const Color color = LookupColor(editor, rule.labels[i]);
					brush->SetColor(color.ToD2D());
					
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
