#pragma once
#include "language/syntaxhighlighter.h"
#include <string>

namespace toml { class node; }
struct TSLanguage;
struct TSQuery;
struct TSQueryCursor;

struct SyntaxHighlighterTreeSitter : public SyntaxHighlighter {

	//-----------------------------------------------------
	// data

	std::string modulePath = {};
	std::string procName = {};
	std::string queryText = {};
	
	void* hModule = nullptr;
	const TSLanguage* language = nullptr;
	const TSQuery* query = nullptr;
	TSQueryCursor* queryCursor = nullptr;
	
	//-----------------------------------------------------
	// functions
	
	static void Cleanup(Editor* editor);
	
	bool FromToml(toml::node* toml);
	~SyntaxHighlighterTreeSitter() noexcept;
	
	virtual void OnOpenFile(Editor* editor, std::string_view buffer) override;
	virtual void OnCloseFile(Editor* editor) override;
	virtual void OnTextBufferChanged(Editor* editor, const TextChange* change) override;
	virtual void Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) override;
};
