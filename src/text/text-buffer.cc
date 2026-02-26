#include "text-buffer.hh"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static TextBuffer::Chunk* AllocateChunk(TextBuffer* self) {
	
	TextBuffer::Chunk* allocated = new TextBuffer::Chunk();
	allocated->prev = self->lastChunk;
	
	ASSERT(self->lastChunk);
	self->lastChunk->next = allocated;
	self->lastChunk = allocated;

	return allocated;
}

static void CreateLines(TextBuffer::Chunk* chunk, /*out*/ std::deque<TextBuffer::Line>* lines, bool keepLastLine) {

	chunk->references = 0u;

	char* lineStart = chunk->data.data();

	char* begin = chunk->data.data();
	char* end   = chunk->data.data() + chunk->data.size();
	
	for (char* it = begin; it != end; /**/) {

		if (*it == '\n') {
			
			lines->push_back(TextBuffer::Line {
				.data = lineStart,
				.length = static_cast<u64>(it - lineStart),
				.lengthLinebreak = 1u,
				.chunk = chunk });

			it++;
			lineStart = it;
			chunk->references++;
		
		} else if (*it == '\r') {
			
			const auto itAfterCR = it + 1;
			if (itAfterCR < end && *itAfterCR == '\n') {

				lines->push_back(TextBuffer::Line {
					.data = lineStart,
					.length = static_cast<u64>(it - lineStart),
					.lengthLinebreak = 2u,
					.chunk = chunk });
				
				it = itAfterCR + 1;
				lineStart = it;
				chunk->references++;
			
			} else {

				lines->push_back(TextBuffer::Line {
					.data = lineStart,
					.length = static_cast<u64>(it - lineStart),
					.lengthLinebreak = 1u,
					.chunk = chunk });

				it++;
				lineStart = it;
				chunk->references++;
			}

		} else {
			it++;
		}
	}

	if (keepLastLine) {
		lines->push_back(TextBuffer::Line {
			.data = lineStart,
			.length = static_cast<u64>(end - lineStart),
			.lengthLinebreak = 0u,
			.chunk = chunk });
		chunk->references++;
	}

	ASSERT(chunk->references != 0u);
	chunk->isShared = (chunk->references > 1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void TextBuffer::Init(std::string initialText /*= {}*/) {
	lastChunk = new Chunk {
		.data = std::move(initialText)};
	
	CreateLines(lastChunk, &lines, true);
}

std::string* TextBuffer::Clear() {
	ASSERT(lastChunk);
		
	Chunk* chunk = lastChunk;
	while (chunk->prev) {
		Chunk* prevChunk = chunk->prev;
		delete chunk;
		chunk = prevChunk;
	}
	
	chunk->data.clear();
	chunk->isShared = false;
	chunk->references = 0u;
	chunk->next = chunk->prev = nullptr;
	
	lastChunk = chunk;
	lines.push_back(Line {
		.data = chunk->data.data(),
		.length = 0,
		.lengthLinebreak = 0,
		.chunk = lastChunk});
	
	return &chunk->data;
}
	
void TextBuffer::RecreateLines() {
	lines.clear();
	CreateLines(lastChunk, &lines, true);
}

TextBuffer::~TextBuffer() noexcept {
	Chunk* chunk = lastChunk;
	while (chunk) {
		Chunk* prevChunk = chunk->prev;
		delete chunk;
		chunk = prevChunk;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const TextBuffer::Line& TextBuffer::GetLineAt(u64 line) const {
	ASSERT(line < lines.size());
	return lines[line];
}

std::string TextBuffer::GetText(const TextPosition& from, const TextPosition& to) const {
	
	std::string result {};

	if (from.line == to.line) {
		const Line& line = GetLineAt(from.line);
		result = std::string {line.GetTextWithLinebreak().substr(from.column, (to.column - from.column))};
		
	} else {	
		const Line &firstLine = GetLineAt(from.line);
		result += firstLine.GetTextWithLinebreak().substr(from.column);
		
		for (u64 ln = from.line + 1; ln < to.line; ln++) {
			const Line& line = GetLineAt(ln);
			result += line.GetTextWithLinebreak();
		}
		
		const Line& lastLine = GetLineAt(to.line);
		result += lastLine.GetText().substr(0, to.column);
	}
	
	return result;
}

std::string TextBuffer::GetText() const {
	std::string result {};
	for (const Line& line : lines)
		result += line.GetTextWithLinebreak();

	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void DecreaseReference(TextBuffer* self, TextBuffer::Chunk* chunk) {

	chunk->references--;

	if (chunk->references == 0u) {
		
		if (chunk->prev)
			chunk->prev->next = chunk->next;

		if (chunk->next)
			chunk->next->prev = chunk->prev;

		if (self->lastChunk == chunk)
			self->lastChunk  = chunk->prev;
		
		delete chunk;
	}
}

static void UpdateLineFromChunk(TextBuffer::Line &line, TextBuffer::Chunk* chunk) {

	ASSERT(chunk->references > 0u);
	line.data = chunk->data.data();
	line.length = chunk->data.length() - line.lengthLinebreak;
	line.chunk = chunk;
}

void TextBuffer::Insert(const TextPosition& where, std::string_view textToInsert, /*out*/ TextChangeOperation* change /*= nullptr*/) {

	ASSERT(where.line >= 0u && where.line < lines.size());
	
	Line*  line  = &lines[where.line];
	Chunk* chunk = line->chunk;

	const u64 lengthTrailingPart = (line->LengthWithLinebreak() - where.column);

	if (chunk->isShared) {
		
		Chunk* newChunk = AllocateChunk(this);
		newChunk->data.reserve(line->LengthWithLinebreak() + textToInsert.length());

		newChunk->data.append(line->data, where.column);
		newChunk->data.append(textToInsert);
		newChunk->data.append(line->data + where.column, lengthTrailingPart);

		DecreaseReference(this, chunk);
		chunk = newChunk;

	} else {
		
		chunk->references = 0u;
		chunk->data.insert(where.column, textToInsert);
	}

	std::deque<Line> newLines {};
	{
		// CreateLines() will give us an extra empty line
		// which we must ignore except when we insert in the last line
		const bool keepLastLine = (where.line == GetMaxLine());

		CreateLines(chunk, &newLines, keepLastLine);
		ASSERT(!newLines.empty());	
	}
	
	*line = newLines.front();
	lines.insert(
		lines.begin() + (where.line + 1),
		newLines.begin() + 1,
		newLines.end());

	if (change) {
		ASSERT(change->removedText.empty() || change->start == where);
	
		change->start = where;
		change->insertionEnd = TextPosition {
			.line   = where.line + newLines.size() - 1,
			.column = newLines.back().LengthWithLinebreak() - lengthTrailingPart };
		change->insertedText = std::string {textToInsert};
	}
}

void TextBuffer::InsertInLine(const TextPosition& where, std::string_view textToInsert, TextChangeOperation* change) {

	Line * line  = &lines[where.line];
	Chunk* chunk =  line->chunk;

	if (chunk->isShared) {
		
		Chunk* newChunk = AllocateChunk(this);
		newChunk->data.reserve(line->LengthWithLinebreak() + textToInsert.length());

		newChunk->data.append(line->data, where.column);
		newChunk->data.append(textToInsert);
		newChunk->data.append(line->data + where.column, line->LengthWithLinebreak() - where.column);
		newChunk->references = 1u;

		DecreaseReference(this, chunk);
		chunk = newChunk;

	} else {
		chunk->data.insert(where.column, textToInsert);
	}

	UpdateLineFromChunk(*line, chunk);
	
	if (change) {
		ASSERT(change->removedText.empty() || change->start == where);
		
		change->start = where;
		change->insertionEnd = TextPosition {where.line, where.column + textToInsert.size()};
		change->insertedText = std::string {textToInsert};
	}
}

void TextBuffer::InsertChunk(u64 linenr, std::string text, /*out*/ TextChangeOperation* change /*= nullptr*/) {
	
	Chunk* chunk = AllocateChunk(this);
	chunk->data = std::move(text);
	
	const bool keepLastLine = (linenr == GetMaxLine());
	
	std::deque<Line> newLines {};
	CreateLines(chunk, &newLines, keepLastLine);
	
	ASSERT(!newLines.empty());
	lines.insert(lines.begin() + linenr, newLines.begin(), newLines.end());
	
	if (change) {
		const TextPosition start = TextPosition {linenr, 0u};
		ASSERT(change->removedText.empty() || change->start == start);
		
		change->start = start;
		change->insertionEnd = TextPosition {linenr + newLines.size(), 0u};
		change->insertedText = chunk->data;
	}
}

void TextBuffer::Remove(const TextPosition& from, const TextPosition& to, /*out*/ TextChangeOperation* change /*= nullptr*/) {

	Line*  fromLine = &lines[from.line];
	Line*  toLine   = &lines[to.line];
	Chunk* chunk    = fromLine->chunk;

	if (change) {
		ASSERT(change->insertedText.empty());
		
		change->start = from;
		change->removalEnd = to;

		if (from.line == to.line) {
			ASSERT(fromLine == toLine);
			change->removedText = std::string {fromLine->data + from.column, to.column - from.column};
		
		} else {
			change->removedText.append(fromLine->data + from.column, fromLine->LengthWithLinebreak() - from.column);
			for (u64 i = from.line+1; i <= to.line-1; i++)
				change->removedText.append(lines[i].data, lines[i].LengthWithLinebreak());
			change->removedText.append(toLine->data, to.column);
		}
	}

	const u64 lengthTrailingPart = (toLine->LengthWithLinebreak() - to.column);

	if (chunk->isShared) {
		
		Chunk* newChunk = AllocateChunk(this);

		newChunk->data.reserve(from.column + lengthTrailingPart);
		newChunk->data.append(fromLine->data, from.column);
		newChunk->data.append(toLine->data + to.column, lengthTrailingPart);
		newChunk->references = 1u;

		DecreaseReference(this, chunk);
		chunk = newChunk;

	} else {
		chunk->data.resize(from.column);
		chunk->data.append(toLine->data + to.column, lengthTrailingPart);
	}

	fromLine->lengthLinebreak = toLine->lengthLinebreak;
	UpdateLineFromChunk(*fromLine, chunk);

	if (from.line != to.line) {

		for (u64 i = from.line + 1; i <= to.line; i++) {
			const Line  &line  = lines[i];
			DecreaseReference(this, line.chunk);
		}

		lines.erase(
			lines.begin() + from.line + 1, // +1 because we don't want to erase the from-line
			lines.begin() + to.line + 1);  // +1 because this arg is exclusive but we want inclusive
	}
}

void TextBuffer::RemoveInLine(u64 linenr, u64 from, u64 to, /*out*/ TextChangeOperation* change /*= nullptr*/) {

	Line*  line  = &lines[linenr];
	Chunk* chunk = line->chunk;

	if (change) {
		ASSERT(change->insertedText.empty());
		
		change->start = TextPosition {linenr, from};
		change->removalEnd = TextPosition {linenr, to};
		change->removedText = std::string {line->data + from, to - from};
	}

	const u64 lengthTrailingPart = (line->LengthWithLinebreak() - to);

	if (chunk->isShared) {
		
		Chunk* newChunk = AllocateChunk(this);

		newChunk->data.reserve(from + lengthTrailingPart);
		newChunk->data.append(line->data, from);
		newChunk->data.append(line->data + to, lengthTrailingPart);
		newChunk->references = 1u;

		DecreaseReference(this, chunk);
		chunk = newChunk;

	} else {
		chunk->data.resize(from);
		chunk->data.append(line->data + to, lengthTrailingPart);
	}

	UpdateLineFromChunk(*line, chunk);
}

void TextBuffer::RemoveChunk(u64 first, u64 last, /*out*/ TextChangeOperation* change /*= nullptr*/) {
		
	ASSERT(first <= last);
	ASSERT(last < lines.size());
	
	if (change) {
		ASSERT(change->insertedText.empty());
				
		change->start = TextPosition {first, 0};
		change->removalEnd = (last != GetMaxLine())
			? TextPosition {last + 1, 0}
			: TextPosition {last, lines.back().LengthWithLinebreak()};
	}
	
	for (u64 ln = first; ln <= last; ln++) {
			
		Line &line = lines[ln];
			
		if (change)
			change->removedText.append(line.GetTextWithLinebreak());

		DecreaseReference(this, line.chunk);
	}
	lines.erase(lines.begin() + first, lines.begin() + last + 1);
}

// @TODO(delete-line)
// void TextBuffer::RemoveLine(u64 linenr, /*out*/ TextEdit* edit /*= nullptr*/) {
	
// 	Line &line = lines[linenr];

// 	if (edit) {
// 	  * edit = TextEdit {
// 			.start = TextPosition {
// 				.line = linenr,
// 				.column = 0 },
// 			.end = TextPosition {
// 				.line = line.LengthWithLinebreak(),
// 				.column = 0 },
// 			.text = std::string_view {}};
// 	}

// 	DecreaseReference(this, line.chunk);
// 	lines.erase(lines.begin() + linenr);
// }
