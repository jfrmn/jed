#pragma once
#include "basic.hh"

struct Editor;
struct ID2D1RenderTarget;

struct SyntaxHighlighter {
	virtual void Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) = 0;
};
