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
	usize capacity = 0u;
	usize count = 0u;
	
	TextChangeOperation single = {};
	
	void ReserveCapacity(usize newCapa);
	void ReserveMore(usize additionalCapa);
	
	TextChangeOperation* NewOperation();
	void Clear();	

	DISALLOW_COPY_AND_ASSING(TextChange);
};
