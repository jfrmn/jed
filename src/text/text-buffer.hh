#pragma once
#include "basic.hh"
#include "text-position.hh"
#include "text-change.hh"

#include <deque>
#include <string>

struct TextBuffer {
	
	//-----------------------------------------------------
	// types

	struct Chunk {
		std::string data = {};

		bool isShared = false;
		u32 references = 0u;

		Chunk* next = nullptr;
		Chunk* prev = nullptr;
	};

	struct Line {
		const char* data = nullptr;
		u64    length = 0u;
		u8     lengthLinebreak = 0u;
		Chunk* chunk = nullptr;

		u64 LengthWithLinebreak() const               { return length + lengthLinebreak; }
		std::string_view GetText() const              { return std::string_view(data, length); }
		std::string_view GetTextWithLinebreak() const { return std::string_view(data, length + lengthLinebreak); }
	};

	//-----------------------------------------------------
	// data

	std::deque<Line> lines = {};
	Chunk* lastChunk  = nullptr;

	//-----------------------------------------------------
	// functions
	
	void Init(std::string initialText = {});
	
	std::string* Clear();
	void RecreateLines();

	const Line& GetLineAt(u64 line) const;
	u64 GetMaxLine() const { return lines.size()-1; }
	u64 LineCount() const { return lines.size(); }

	std::string GetText(const TextPosition& from, const TextPosition& to) const;
	std::string GetText() const;

	//-----------------------------------------------------
	// text modification function

	void Insert(const TextPosition& where, std::string_view textToInsert, /*out*/ TextChangeOperation* change = nullptr);
	void InsertInLine(const TextPosition& where, std::string_view textToInsert, /*out*/ TextChangeOperation* change = nullptr);
	void InsertChunk(u64 linenr, std::string text, TextChangeOperation* change = nullptr);

	void Remove(const TextPosition& from, const TextPosition& to, /*out*/ TextChangeOperation* change = nullptr);
	void RemoveInLine(u64 linenr, u64 from, u64 to, /*out*/ TextChangeOperation* change = nullptr);
	void RemoveChunk(u64 first, u64 last, /*out*/ TextChangeOperation* change = nullptr);

	DISALLOW_COPY_AND_ASSING(TextBuffer)
};