#include "syntaxhighlighter.hh"
#include "editor/editor.hh"
#include "language.hh"
#include "settings.hh"

void SyntaxHighlighter::OnOpenFile(Editor* editor, std::string_view buffer) {}
void SyntaxHighlighter::OnTextBufferChanged(Editor* editor, const TextChange* change) {}
void SyntaxHighlighter::OnCloseFile(Editor* editor) {}

Color SyntaxHighlighter::LookupColor(const Editor* editor, std::string_view label) {

	const u64 hash = HashString(label);
	
	if (const Color* color = settings.syntaxColors.LookupWithHash(label, hash))
		return *color;

	const Language* language = editor->language;
	ASSERT(language);
	
	if (const Color* color = language->syntaxColors.LookupWithHash(label, hash))
		return *color;
	
	if      (label == "keyword")      return Color::FromKnown(D2D1::ColorF::RoyalBlue);
	else if (label == "function")     return Color::FromKnown(D2D1::ColorF::LemonChiffon);
	else if (label == "control-flow") return Color::FromKnown(D2D1::ColorF::RoyalBlue);
	else if (label == "string")       return Color::FromKnown(D2D1::ColorF::LightSalmon);
	else if (label == "comment")      return Color::FromKnown(D2D1::ColorF::LightGray);
	else if (label == "type")         return Color::FromKnown(D2D1::ColorF::DarkTurquoise);
	else if (label == "preprocessor") return Color::FromKnown(D2D1::ColorF::HotPink);
	else if (label == "number")       return Color::FromKnown(D2D1::ColorF::LimeGreen);
	else if (label == "tag")          return Color::FromKnown(D2D1::ColorF::Gold);
	else return COLOR_WHITE;
}
