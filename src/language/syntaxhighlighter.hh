#pragma once
#include "basic.hh"
#include <string_view>

union Color;
struct Editor;
struct Language;
struct ID2D1RenderTarget;
struct TextChange;

struct SyntaxHighlighter {
	virtual void OnOpenFile(Editor* editor, std::string_view buffer);
	virtual void OnTextBufferChanged(Editor* editor, const TextChange* change);
	virtual void OnCloseFile(Editor* editor);
	virtual void Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) = 0;
	
	Color LookupColor(const Editor* editor, std::string_view label);
};
