#include "text-change.hh"

void TextChangeOperation::Clear() {
	start = TextPosition {};
	insertionEnd = TextPosition {};
	removalEnd  = TextPosition {};
	
	insertedText.clear();
	removedText.clear();
}

void TextChangeOperation::AdjustPosition(TextPosition& position) const {
	
	if (start.line == position.line && start.character < position.character) {
		if (!removedText.empty())
			position.character -= removalEnd.character - start.character;
						
		if (!insertedText.empty())
			position.character += insertionEnd.character - start.character;
	}
	
	if (start.line <= position.line) {
		if (!removedText.empty())
			position.line -= removalEnd.line - start.line;
			
		if (!insertedText.empty())
			position.line += insertionEnd.line - start.line;
	}
}
	
void TextChange::ReserveCapacity(usize newCapa) {
	// we could accept this as valid and just return
	ASSERT(newCapa != 0u);

	if (newCapa <= capacity) {
		return;

	} else if (newCapa == 1u) {
		ASSERT(!operations);
		operations = &single;
		capacity = 1u;
	
	} else {
		auto newOperations = new TextChangeOperation[newCapa];
		std::move(operations, operations + capacity, newOperations);

		if (operations != &single)
			delete[] operations;

		operations = newOperations;
		capacity = newCapa;
	}
}

void TextChange::ReserveMore(usize additionalCapa) {
	ReserveCapacity(count + additionalCapa);
}

TextChangeOperation* TextChange::NewOperation() {
	
	ReserveMore(1u);

	TextChangeOperation* rec = &operations[count];
	count++;

	return rec;
}

void TextChange::Clear() {
	for (usize i = 0u; i < count; i++)
		operations[i].Clear();

	count = 0u;
}

TextChange::~TextChange() noexcept {
	if (operations != &single)
		delete[] operations;
}
