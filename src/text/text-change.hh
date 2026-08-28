#pragma once
#include "text-position.hh"

#include <string>

struct TextChangeOperation {
	TextPosition start = {};
	TextPosition insertionEnd = {};
	TextPosition removalEnd  = {};
	
	std::string insertedText = {};
	std::string removedText  = {};
	
	void Clear();
	void AdjustPosition(TextPosition& position) const;
};

struct TextChange {

	TextChangeOperation* operations = {};
	u64 capacity = 0u;
	u64 count = 0u;
	
	TextChangeOperation single = {};
	
	void ReserveCapacity(u64 newCapa);
	void ReserveMore(u64 additionalCapa);
	
	TextChangeOperation* NewOperation();
	void Clear();
	
	~TextChange() noexcept;
};
