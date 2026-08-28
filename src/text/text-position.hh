#pragma once
#include "basic.hh"

struct TextPosition {
	u64 line      = 0u;
	u64 character = 0u;

	bool operator< (const TextPosition& other) const;
	bool operator<=(const TextPosition& other) const;
	bool operator> (const TextPosition& other) const;
	bool operator>=(const TextPosition& other) const;
	bool operator==(const TextPosition& other) const;
};

template<class TCallable>
void IterateTextRange(const TextPosition& from, const TextPosition& to, TCallable callback) {
	
	if (from.line == to.line) {
		callback(from.line, from.character, to.character);
	
	} else {
		
		// first line
		callback(from.line, from.character, U64_MAX);
		
		// lines in between
		for (u64 ln = from.line + 1u; ln < to.line; ln++)
			callback(ln, 0, U64_MAX);
			
		// last line
		callback(to.line, 0, to.character);
	}
}
