#include "text-controller.hh"
#include "commands.hh"
#include "logging.hh"
#include "util.hh"
#include "events.hh"

#include "util/parameter.hh"
#include "editor/editor.hh"

// need hWnd for Clipboard operations
#include "ui/window.hh"

// for toggle comment logic
#include "language/language.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define CLIPBOARD_FORMAT_MULTICARET_TEXT "multi-caret-text"
struct MultiCaretClipboardData {
	struct CaretData {
		u64 length = 0u;
		char* data = nullptr;
	};
	
	u64 caretCount = 0u;
	CaretData data[1];
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TextController::Caret::GetSelection(/*out*/ TextPosition* from, /*out*/ TextPosition* to) const {
	if (!hasSelection) return false;
	
	if (position < selection) {
		*from = position;
		*to   = selection;
	} else {
		*from = selection;
		*to   = position;
	}
	
	return true;
}

void TextController::Caret::ResetSelection() {
	selection = TextPosition {};
	hasSelection = false;
}

bool TextController::Caret::operator<(const Caret& other) const {
	return position < other.position;		
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool TextController::InitForTextbox(std::string initialText) {
	
	ownerEditor = nullptr;
	carets.push_back(Caret {
		.position = TextPosition { 
			.line = 0u,
			.character = initialText.size()},
		.selection = TextPosition {},
		.hasSelection = false});

	buffer.Init(std::move(initialText));
	history.Init(32);
			
	return true;
}

bool TextController::InitForEditor(Editor* owner) {
	
	ownerEditor = owner;
	buffer.Init();
	history.Init(32);	
	
	return true;
}

void TextController::Reset() {
	SetCaretPosition(TextPosition {});
	
	history.Reset();
	historyUndoIndex = U64_MAX;
	
	previousEditWasSignleChar = false;
}

bool TextController::HasSelection() const {
	for (const Caret& caret : carets)
		if (caret.hasSelection) return true;
	return false;
}

bool TextController::GetSelection(TextPosition* from, TextPosition* to) const {
	for (const Caret& caret : carets)
		if (caret.GetSelection(from, to)) return true;
 	return false;
}

void TextController::SetCaretPosition(TextPosition pos) {
	carets.resize(1u);
	carets.front().position = pos;
	carets.front().ResetSelection();
	isEditCaretsMode = false;
	editCaretsPosition = {};
}

void TextController::ToggleCaret() {
	ASSERT(isEditCaretsMode);
	
	for (auto it = carets.begin(); it != carets.end(); ++it) {
		
		// remove caret under the edit-caret
		if (it->position == editCaretsPosition) {
			carets.erase(it);
			return;
		}
		
		// check if we are inside a selecetion - adding carets there is not allowed
		TextPosition itFrom, itTo;
		if (it->GetSelection(&itFrom, &itTo)) {
			if (editCaretsPosition > itFrom &&
				editCaretsPosition < itTo)
				return;
		}
	
		// @FIXME should be > shoulld it not?
		// Otherwise we insert a new caret. The carets are sorted by their position.
		// check if this is the corret index to insert the new caret
		if (it->position < editCaretsPosition) {
			carets.insert(it, TextController::Caret {
				.position = editCaretsPosition,
				.selection = TextPosition {},
				.hasSelection = false});
			return;
		}
	}
	
	// insert at the end	
	carets.push_back(TextController::Caret {
		.position = editCaretsPosition,
		.selection = TextPosition {},
		.hasSelection = false});	
}

void TextController::SetSelection(TextPosition from, TextPosition to) {
	carets.resize(1u);
	carets.front().position = to;
	carets.front().selection = from;
	carets.front().hasSelection = true;
}

TextChange* TextController::NewTextChange() {
		
	historyUndoIndex = USIZE_MAX;
		
	TextChange* newChange = 	history.Push();
	newChange->Clear();
	
	return newChange;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Helper
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Indentation

static u64 GetIndentationEnd(TextController* self, u64 linenr) {

	const TextBuffer::Line& line = self->buffer.GetLineAt(linenr);

	u64 col = 0;
	for (; col < line.length; col++) {
		if (std::isspace(line.data[col]) != 0) {
			return col;
		}
	}

	return 0u;
}

static u64 GetVisualColumn(TextController* self, const TextPosition* point) {
	
	u64 visualColumn = 0u;
	
	const TextBuffer::Line& line = self->buffer.GetLineAt(point->line);
	for (u64 i = 0u; i < point->character; i++) {
		
		const char ch = line.data[i];
		
		if (IsMultibyteCodepointMember(ch)) {
			visualColumn += 0;
		} else if (ch == '\t') {
			visualColumn += 4;
			u64 rest = visualColumn % 4;
			visualColumn -= rest;
		} else {
			visualColumn += 1;
		}
	}
	
	return visualColumn;
}

static void MoveToVisualColumn(TextController* self, TextPosition* point, u64 targetVisualColumn) {
	
	const TextBuffer::Line& line = self->buffer.GetLineAt(point->line);
	
	u64 currentVisualColumn = 0u;
	u64 i = 0u;
	while (true) {
		
		if (currentVisualColumn >= targetVisualColumn) break;
		if (i >= line.length) break;

		const char ch = line.data[i];
		
		if (IsMultibyteCodepointMember(ch)) {
			currentVisualColumn += 0;
		} else if (ch == '\t') {
			currentVisualColumn += 4;
			u64 rest = currentVisualColumn % 4;
			currentVisualColumn -= rest;
		} else {
			currentVisualColumn += 1;
		}
		
		++i;
	}

	ASSERT(i <= line.length);
	point->character = i;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Adjusting carets

static void AdjustFollowingCarets(TextController* self, u64 currentCaretIndex, const TextChangeOperation* operation) {
	ASSERT(operation);
			
	for (u64 i = currentCaretIndex + 1u; i < self->carets.size(); i++) {
		TextController::Caret& caret = self->carets[i];
				
		operation->AdjustPosition(caret.position);
		if (caret.hasSelection)
			operation->AdjustPosition(caret.selection);
	}	
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Movements
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void MovePrevChar(TextController* self, TextPosition* position) {

	// wrap around line start
	if (position->character == 0u) {

		// respect buffer start
		if (position->line == 0u) {
			return;
		}

		// set codepoint to line end
		position->line--;
		position->character = self->buffer.GetLineAt(position->line).length;

	} else {

		const TextBuffer::Line& line = self->buffer.GetLineAt(position->line);
		position->character--;

		while (IsMultibyteCodepointMember(line.data[position->character]))
			position->character--;
	}
}

static void MoveNextChar(TextController* self, TextPosition* position) {

	// wrap around line ending
	if (position->character == self->buffer.GetLineAt(position->line).length) {
		
		// respect buffer end
		if (position->line  == self->buffer.GetMaxLine()) {
			position->character = self->buffer.lines.back().length;
			return;
		}

		position->line++;
		position->character = 0;
	
	} else {
	
		const TextBuffer::Line &line = self->buffer.GetLineAt(position->line);
		position->character++;

		while (IsMultibyteCodepointMember(line.data[position->character]))
			position->character++;
	}
}

static void MoveLineUp(TextController* self, TextPosition* position) {
	
	if (position->line == 0u) {
		position->character = 0u;
		return;
	}
	const u64 visualColumn = GetVisualColumn(self, position);
	position->line--;
	MoveToVisualColumn(self, position, visualColumn);
}

static void MoveLineDown(TextController* self, TextPosition* position) {

	if (position->line == self->buffer.GetMaxLine()) {
		position->character = self->buffer.GetLineAt(position->line).length;
		return;
	}

	const u64 visualColumn = GetVisualColumn(self, position);
	position->line++;
	MoveToVisualColumn(self, position, visualColumn);
}

static void MoveToNextWord(TextController* self, TextPosition* position) {

	const TextBuffer::Line& line = self->buffer.GetLineAt(position->line);

	if (position->character == line.length) {
		
		if (position->line != self->buffer.GetMaxLine()) {
			position->line++;	
			position->character = 0u;	
		} else {
			position->character = line.length;
		}

		return;
	}

	char currentChar = line.data[position->character];
	const int wasAlphaNumeric = std::isalnum(currentChar);

	// @FIXME does not respect multibyte-codepoints

	while (std::isalnum(currentChar) == wasAlphaNumeric
		|| IsMultibyteCodepointMember(currentChar)) {

		position->character++;
		if (position->character == line.length)
			return;

		currentChar = line.data[position->character];
	}
}

static void MoveToPrevWord(TextController* self, TextPosition* position) {

	if (position->character == 0u) {
		
		if (position->line != 0u) {
			position->line--;
			position->character = self->buffer.GetLineAt(position->line).length;
		} else {
			position->character = 0u;
		}

		return;
	}

	const TextBuffer::Line &line = self->buffer.GetLineAt(position->line);

	u64 prevColumn = position->character - 1;
	char prevChar = line.data[prevColumn];

	const int wasAlphaNumeric = std::isalnum(prevChar);

	while (std::isalnum(prevChar) == wasAlphaNumeric
		|| IsMultibyteCodepointMember(prevChar)) {
		
		if (prevColumn == 0u) {
			position->character = 0u;
			return;
		}

		position->character = prevColumn;
		prevColumn = position->character - 1;
		prevChar = line.data[prevColumn];
	}
}

static void MoveToLineStart(TextController* self, TextPosition* position) {

	const u64 col = GetIndentationEnd(self, position->line);
	position->character = col;
}

static void MoveToLineEnd(TextController* self, TextPosition* position) {

	const TextBuffer::Line &line = self->buffer.GetLineAt(position->line);
	position->character = line.length;
}

static void MoveToBufferStart(TextController* self, TextPosition* position) {

	*position = TextPosition {0u, 0u};
	if (self->ownerEditor)
		self->ownerEditor->ScrollToLine(0u);
}

static void MoveToBufferEnd(TextController* self, TextPosition* position) {
	
	const u64 maxLine = self->buffer.GetMaxLine();
	*position = TextPosition {maxLine, self->buffer.lines.back().length};
	if (self->ownerEditor)
		self->ownerEditor->ScrollToLine(maxLine);
}

static void MovePageUp(TextController* self, TextPosition* position) {
	
	if (self->ownerEditor) {
		
		u64 firstVisibleLine = 0u;
		self->ownerEditor->GetVisibleLines(&firstVisibleLine, nullptr);
		
		if (firstVisibleLine == 0u) {
			MoveToBufferStart(self, position);
			return;
		}
			
		firstVisibleLine--;
		
		*position = TextPosition {
			.line = firstVisibleLine,
			.character = GetIndentationEnd(self, firstVisibleLine) };
			
		self->ownerEditor->ScrollToLine(firstVisibleLine);
			
	} else {
		MoveToLineStart(self, position);
	}
}

static void MovePageDown(TextController* self, TextPosition* position) {
	
	if (self->ownerEditor) {
	
		u64 lastVisibleLine = 0u;
		self->ownerEditor->GetVisibleLines(nullptr, &lastVisibleLine);
		
		if (lastVisibleLine >= self->buffer.GetMaxLine()) {
			MoveToBufferEnd(self, position);
			return;
		}
			
		lastVisibleLine++;
		
		*position = TextPosition {
			.line = lastVisibleLine,
			.character = GetIndentationEnd(self, lastVisibleLine) };
			
		self->ownerEditor->ScrollToLine(lastVisibleLine);
			
	} else {
		MoveToLineEnd(self, position);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Basic operations
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static void InsertText(TextController* self, std::string_view text, /*out*/ TextChange** outChange) {
	
	TextChange* change = *outChange = self->NewTextChange();
	change->ReserveCapacity(self->carets.size());
	
	for (u64 i = 0u; i < self->carets.size(); i++) {	
		TextController::Caret& caret = self->carets[i];
		TextChangeOperation* operation = change->NewOperation();
		
		TextPosition insertion;
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			self->buffer.Remove(from, to, operation);
			caret.ResetSelection();
			insertion = from;
		} else {
			insertion = caret.position;
		}
		
		self->buffer.InsertInLine(insertion, text, operation);
		caret.position = operation->insertionEnd;
		
		AdjustFollowingCarets(self, i, operation);
	}
}

static void MoveCaret(TextController* self, void (*funcMovement)(TextController*, TextPosition*)) {
	for (TextController::Caret& caret : self->carets) {
		caret.ResetSelection();
		funcMovement(self, &caret.position);
	}
}

static void Select(TextController* self, void (*funcMovement)(TextController*, TextPosition*)) {
	for (TextController::Caret& caret : self->carets) {
		if (!caret.hasSelection)	{
		 	 caret.selection = caret.position;
		 	 caret.hasSelection = true;
		}
		funcMovement(self, &caret.position);
	}
}

static void Delete(TextController* self, TextChange** outChange, void (*funcMovement)(TextController*, TextPosition*)) {
	
	TextChange* change = *outChange = self->NewTextChange();
	change->ReserveCapacity(self->carets.size());
	
	for (TextController::Caret& caret : self->carets) {
		
		TextPosition from, to;
		if (!caret.GetSelection(&from, &to)) {
			TextPosition other = caret.position;
			funcMovement(self, &other);
			if (other < caret.position) {
				from = other;
				to = caret.position;
			} else {
				from = caret.position;
				to = other;
			}
		}
		
		self->buffer.Remove(from, to, change->NewOperation());
		caret.ResetSelection();
		caret.position = from;
	}
}

//static void ActionDeleteRight(TextController* self, TextChange** change, void (*funcMovement)(TextController*, TextPosition*)) {
//
//	self->InitTextChange(change);
//	(*change)->ReserveCapacity(self->carets.size());
//	
//	for (TextController::Caret& caret : self->carets) {
//		TextPosition from, to;
//		if (!caret.GetSelection(&from, &to)) {
//			from = caret.position;
//			funcMovement(self, &caret.position);
//			to = caret.position;
//		}
//	
//		self->buffer.Remove(from, to, (*change)->NewOperation());
//		caret.ResetSelection();
//		caret.position = from;
//	}
//}

static void InsertNewLine(TextController* self, TextChange** outChange) {
	
	TextChange* change = *outChange = self->NewTextChange();
	change->ReserveCapacity(self->carets.size());
	
	for (TextController::Caret& caret : self->carets) {
		
		TextChangeOperation* operation = change->NewOperation();
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			self->buffer.Remove(from, to, operation);
			caret.ResetSelection();
			caret.position = from;
		}
	
		// @TODO(settings) use line-ending mode
		std::string toInsert {"\r\n"};
	
		// adjust to current indentation
		{
			const TextBuffer::Line& line = self->buffer.GetLineAt(caret.position.line);
			
			const u64 maxColumn = std::min(line.length, caret.position.character);
			for (u64 i = 0u; i < maxColumn; i++) {
				
				const char ch = line.data[i];
				if (ch == '\t' || ch == ' ')
					toInsert.push_back(ch);
				else
					break;
			}
		}
		
		self->buffer.Insert(caret.position, toInsert, operation);
		caret.position = operation->insertionEnd;
	}
}

static void ClearMultiCarets(TextController* self, bool keepLast) {
	if (keepLast) std::swap(self->carets.front(), self->carets.back());
	self->carets.resize(1u);
}

static void EnterEditMultiCaretMode(TextController* self, bool spawnAtLast) {
	ASSERT(!self->isEditCaretsMode);
	
	self->isEditCaretsMode = true;
	self->editCaretsPosition = spawnAtLast
		? self->carets.back().position
		: self->carets.front().position;
}

static void LeaveEditMultiCaretMode(TextController* self) {
	ASSERT(self->isEditCaretsMode);
	self->isEditCaretsMode = false;	
	self->editCaretsPosition = {};
}

static void ShiftCaret(TextController* self, void (*funcMovement)(TextController*, TextPosition*)) {
	TextPosition* positionToModifiy = nullptr;
	for (TextController::Caret& caret : self->carets) {
		if (caret.position == self->editCaretsPosition) {
			positionToModifiy = &caret.position;
			goto found_position;
		}
		
		if (caret.hasSelection && caret.selection == self->editCaretsPosition) {
			positionToModifiy = &caret.selection;
			goto found_position;
		}
	}	
	return;
	
found_position:
	funcMovement(self, positionToModifiy);
	self->editCaretsPosition = *positionToModifiy;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Commands
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Basic

static void CommandMoveCaret(TextController* self, std::span<ParameterValue> params) {
	const TextPosition target {
		.line = static_cast<u64>(params[0].numberValue),
		.character = static_cast<u64>(params[1].numberValue)};
	
	if (self->isEditCaretsMode) {
		self->editCaretsPosition = target;
	} else {
		self->carets.resize(1u);
		self->carets.front().ResetSelection();
		self->carets.front().position = target;
	}
}

static void CommandInsertText(TextController* self, std::span<ParameterValue> params, TextChange** outChange) {
	InsertText(self, params.front().stringValue, outChange);
	// @FIXME works only with text that does not contain any linebreaks
}

static void CommandDeleteRange(TextController* self, std::span<ParameterValue> params, TextChange** outChange) {
	const TextPosition from {
		.line = static_cast<u64>(params[0].numberValue),
		.character = static_cast<u64>(params[1].numberValue)};
	const TextPosition to {
		.line = static_cast<u64>(params[2].numberValue),
		.character = static_cast<u64>(params[3].numberValue)};
	
	TextChange* change = *outChange = self->NewTextChange();
	self->buffer.Remove(from, to, change->NewOperation());
	
	// @TODO finish
	ASSERT_NOT_IMPLEMENTED;
	
	for (TextController::Caret& caret : self->carets) {
		
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Selection

static void CommandSelectAll(TextController* self) {
	self->carets.resize(1u);	
	self->carets.front().hasSelection = true;	
	self->carets.front().selection = TextPosition {0u, 0u};
	MoveToBufferEnd(self, &self->carets.front().position);
}

static void CommandSelectLine(TextController* self) {
	for (TextController::Caret& caret : self->carets) {
		caret.hasSelection = true;
		caret.selection = TextPosition {
			.line = caret.position.line,
			.character = GetIndentationEnd(self, caret.position.line)};
		MoveToLineEnd(self, &caret.position);
	}
}

static void CommandSelectInBrackets(TextController* self) {
	for (TextController::Caret& caret : self->carets) {
		
		const TextBuffer::Line& line = self->buffer.GetLineAt(caret.position.line);
		s64 start = static_cast<s64>(caret.position.character);
		char bracket = '\0';
		for (; start >= 0u; start--) {
			if (line.data[start] == '(' || line.data[start] == '{' || line.data[start] == '[') {
				bracket = line.data[start];
				goto found_bracket;
			}
		}
		start = GetIndentationEnd(self, caret.position.line);
		
	found_bracket:
		start++;
		ASSERT(start >= 0u);
		
		u64 end = caret.position.character;
		for (; end < line.length; end++) {
			if (bracket == '\0' && (line.data[end] == ')' || line.data[end] == '}' || line.data[end] == ']')) break;
			else if (bracket == '(' && line.data[end] == ')') break;
			else if (bracket == '{' && line.data[end] == '}') break;
			else if (bracket == '[' && line.data[end] == ']') break;
		}
		
		caret.hasSelection = true;
		caret.selection.character = start;
		caret.selection.line = caret.position.line;
		caret.position.character = end;		
	}
}

static void CommandSelectWord(TextController* self) {
	for (TextController::Caret& caret : self->carets) {
		caret.hasSelection = true;
		MoveToNextWord(self, &caret.position);
			
		caret.selection = caret.position;
		MoveToPrevWord(self, &caret.selection);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Modification

static void CommandRepeatText(TextController* self, std::span<ParameterValue> params, TextChange** change) {
	const std::string_view textToRepeat = params[0].stringValue;
	const u64 repeatCount = static_cast<u64>(params[1].numberValue);
	
	std::string textToInsert {};
	textToInsert.reserve(repeatCount * textToRepeat.size());
	for (u64 i = 0u; i < repeatCount; i++)
		textToInsert.append(textToRepeat);
		
	InsertText(self, textToInsert, change);		
}

static void CommandTransformCase(TextController* self, std::span<ParameterValue> params, TextChange** change) {
	
	const s64 targetCase = params[0].numberValue;
	const bool capitalizeFirstLetter = params[1].numberValue;
	
	for (TextController::Caret& caret : self->carets) {
		TextPosition from, to;
		if (!caret.GetSelection(&from, &to)) {
			from = to = caret.position;
			MoveToNextWord(self, &to);
			MoveToPrevWord(self, &from);
		}
		
		const std::string originalText = self->buffer.GetText(from, to);
		ASSERT(originalText.empty());
		
		std::vector<std::string_view> tokens {};
		
		bool prevIsUpperCase = (originalText.front() >= 'A' && originalText.front() <= 'Z');
		u64 tokenStart = 0u;
		for (u64 i = 0u; i < originalText.size(); i++) {
			const bool isUpperCase = (originalText.front() >= 'A' && originalText.front() <= 'Z');
			if (prevIsUpperCase != isUpperCase) {
				tokens.push_back(std::string_view {originalText.begin() + tokenStart, originalText.begin() + i});
				prevIsUpperCase = isUpperCase;
				continue;
				}
		}
		
		// @TODO finish
	}
}

static void CommandToUpperOrLowerCase(TextController* self, TextChange** outChange, int (*funcTransform)(int)) {
	
	TextChange* change = *outChange = self->NewTextChange();
		
	for (TextController::Caret& caret : self->carets) {
		
		TextPosition from, to;
		
		if (!caret.GetSelection(&from, &to)) {
			from = to = caret.position;
			MoveNextChar(self, &to);
		}
		
		TextChangeOperation* operation = change->NewOperation();
		self->buffer.Remove(from, to, operation);
		
		std::string textToInsert {};
		textToInsert.reserve(operation->removedText.size());
		for (char ch : operation->removedText)
			textToInsert.push_back(static_cast<char>(funcTransform(ch)));
		
		self->buffer.Insert(from, textToInsert, operation);
	}
}

static void CommandDeleteLine(TextController* self, TextChange** outChange) {

	TextChange* change = *outChange = self->NewTextChange();
	change->ReserveCapacity(self->carets.size());	
	
	for (TextController::Caret& caret : self->carets) {	
		u64 lineFrom = 0u, lineTo = 0u;
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			lineFrom = from.line;
			lineTo = to.line;
		} else {
			lineFrom = lineTo = caret.position.line;
		}		
	
		
		self->buffer.RemoveChunk(lineFrom, lineTo, change->NewOperation());
	
		caret.position.character = GetIndentationEnd(self, lineFrom);
		caret.ResetSelection();
	}
}

static void CommandIndentLine(TextController* self, TextChange** outChange) {

	TextChange* change = *outChange = self->NewTextChange();
	
	for (TextController::Caret& caret : self->carets) {	
		
		// @TODO(tabmode)	
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
		
			const u64 lineFrom = std::min(caret.position.line, caret.selection.line);
			const u64 lineTo   = std::max(caret.position.line, caret.selection.line);
			change->ReserveMore((lineTo - lineFrom) + 1);	
		
			for (u64 ln = lineFrom; ln <= lineTo; ln++) {
							
				// @TODO(tabmode)
				self->buffer.InsertInLine(
					TextPosition {
						.line = ln,
						.character = GetIndentationEnd(self, ln) },
					"\t",
					change->NewOperation());
			}
					
			caret.position.character++;
			caret.selection.character++;		
		
		} else {
		
			// @TODO(tabmode)
			self->buffer.InsertInLine(
				TextPosition {
					.line = caret.position.line,
					.character = GetIndentationEnd(self, caret.position.line) },
				"\t",
				change->NewOperation());
				
			caret.position.character++;
		}
	}
}

static void UnindentLine(TextController* self, TextController::Caret& caret, TextChange* change, u64 ln) {
	const std::string_view line = self->buffer.GetLineAt(ln).GetText();
		
	u64 charsToRemove = 0;
		
	// if we find a tab we can just remove the tab - easy
	if (line.starts_with('\t')) {
		charsToRemove = 1;
		
	// we we find a space we try to remove up to tab-size spaces
	} else if (line.starts_with(' ')) {
			
		// @TODO(settings) respect tab-size
		for (/**/; charsToRemove < 4; charsToRemove++) {
			if (charsToRemove >= line.size()) break;
			if (line[charsToRemove] != ' ') break;
		}
		
	} else {
		return;
	}
	
	self->buffer.RemoveInLine(ln, 0, charsToRemove, change->NewOperation());
	
	// adjust cursor
	if (caret.position.line == ln)
		caret.position.character = caret.position.character - std::min(charsToRemove, caret.position.character);
				
	// adjust selection (if needed)
	if (caret.hasSelection && caret.selection.line == ln)
		caret.selection.character = caret.selection.character - std::min(charsToRemove, caret.selection.character);
}

static void CommandUnindentLine(TextController* self, TextChange** outChange) {

	TextChange* change = *outChange = self->NewTextChange();
	
	for (TextController::Caret& caret : self->carets) {

		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
		
			change->ReserveMore((to.line - from.line) + 1u);
			for (u64 ln = from.line; ln <= to.line; ln++)
				UnindentLine(self, caret, change, ln);
		
		} else {
			UnindentLine(self, caret, change, caret.position.line);	
		}
	}
}	

static void CommandDuplicateLine(TextController* self, TextChange** outChange) {
	// @FIXME this doesn't work with the last line yet

	TextChange* change = *outChange = self->NewTextChange();
	
	if (self->ownerEditor)
		self->ownerEditor->PrepareInsertAnimation(self->carets.size());
	
	for (TextController::Caret& caret : self->carets) {
	
		TextChangeOperation* operation = change->NewOperation();	
	
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			std::string toInsert {};
			for (u64 ln = from.line; ln <= to.line; ln++) {
				const TextBuffer::Line& line = self->buffer.GetLineAt(ln);
				toInsert.append(line.GetTextWithLinebreak());
			}
			
			self->buffer.InsertChunk(from.line, std::move(toInsert), operation);
			
			const u64 lineOffset = (to.line - from.line) + 1u;
			caret.position.line  += lineOffset;
			caret.selection.line += lineOffset;	
		
		} else {
		
			const TextBuffer::Line& line = self->buffer.GetLineAt(caret.position.line);
		
			self->buffer.InsertChunk(caret.position.line, std::string {line.GetTextWithLinebreak()}, operation);
			caret.position.line++;
		}
		
		if (self->ownerEditor)
			self->ownerEditor->AddInsertAnimationData(operation->start, operation->insertionEnd);
	}
			
	if (self->ownerEditor)
		self->ownerEditor->StartInsertAnimation();
}

static void CommandSwapLines(TextController* self, TextChange** change) {
	ASSERT_NOT_IMPLEMENTED;
}

static void CommandTrimTrailingWhitespace(TextController* self, TextChange** change) {
	ASSERT_NOT_IMPLEMENTED;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Undo Redo

static void CommandUndo(TextController* self, TextChange** change) {
	
	// no actions done yet - no undo
	if (self->history.written == 0u)
		return;
	
	// prevent underflow
	if (self->historyUndoIndex == 0u)
		return;

	if (self->historyUndoIndex == USIZE_MAX)
		self->historyUndoIndex = self->history.written;
	
	const u64 numberOfUndosDone = (self->history.written - self->historyUndoIndex);
	ASSERT(numberOfUndosDone <= self->history.capacity);

	// can't do more undos than slots available
	if (numberOfUndosDone == self->history.capacity)
		return;

	self->historyUndoIndex--;
	const usize actualIndex = self->historyUndoIndex % self->history.capacity;

	const TextChange* changeToUndo = &self->history.data[actualIndex];
	self->historyUndoTextChange.Clear();
	self->historyUndoTextChange.ReserveCapacity(changeToUndo->count);
	
	if (self->ownerEditor)
		self->ownerEditor->PrepareInsertAnimation();

	for (s64 i = static_cast<s64>(changeToUndo->count) - 1; i >= 0; i--) {

		const TextChangeOperation& operationToUndo = changeToUndo->operations[i];
		TextChangeOperation* undoOperation = self->historyUndoTextChange.NewOperation();

		if (!operationToUndo.insertedText.empty()) {
			self->buffer.Remove(operationToUndo.start, operationToUndo.insertionEnd, undoOperation);
			
			ASSERT(undoOperation->start == operationToUndo.start);
			ASSERT(undoOperation->removalEnd == operationToUndo.insertionEnd);
			ASSERT(undoOperation->removedText == operationToUndo.insertedText);
		}
		
		if (!operationToUndo.removedText.empty()) {
			self->buffer.Insert(operationToUndo.start, operationToUndo.removedText, undoOperation);
			if (self->ownerEditor)
				self->ownerEditor->AddInsertAnimationData(undoOperation->start, undoOperation->insertionEnd);

			ASSERT(undoOperation->start == operationToUndo.start);
			ASSERT(undoOperation->insertionEnd == operationToUndo.removalEnd);
			ASSERT(undoOperation->insertedText == operationToUndo.removedText);
		}
	}
	
	// @TODO this might be possible?
	ASSERT(self->historyUndoTextChange.count > 0u);
	
	self->carets.resize(1u);
	self->carets.front().ResetSelection();
	self->carets.front().position = self->historyUndoTextChange.operations[0].start;

	*change = &self->historyUndoTextChange;
	if (self->ownerEditor)
		self->ownerEditor->StartInsertAnimation();
}

static void CommandRedo(TextController* self, TextChange** change) {

	// did no undos yet
	if (self->historyUndoIndex == USIZE_MAX)
		return;

	// reached the start again
	if (self->historyUndoIndex == self->history.written)
		return;

	const usize actualIndex = self->historyUndoIndex % self->history.capacity;
	TextChange* changeToRedo = &self->history.data[actualIndex];

	if (self->ownerEditor)
		self->ownerEditor->PrepareInsertAnimation();

	for (usize i = 0u; i < changeToRedo->count; i++) {

		const TextChangeOperation& operationToRedo = changeToRedo->operations[i];
		
		TextChangeOperation doneOperation {};
		if (!operationToRedo.removedText.empty() ) {
			
			self->buffer.Remove(operationToRedo.start, operationToRedo.removalEnd, &doneOperation);
			
			ASSERT(doneOperation.start == operationToRedo.start);
			ASSERT(doneOperation.removalEnd == operationToRedo.removalEnd);
			ASSERT(doneOperation.removedText == operationToRedo.removedText);

		} else {
			self->buffer.Insert(operationToRedo.start, operationToRedo.insertedText, &doneOperation);			
			if (self->ownerEditor)
				self->ownerEditor->AddInsertAnimationData(doneOperation.start, doneOperation.insertionEnd);
				
			ASSERT(doneOperation.start == operationToRedo.start);
			ASSERT(doneOperation.insertionEnd == operationToRedo.insertionEnd);
			ASSERT(doneOperation.insertedText == operationToRedo.insertedText);
		}
	}

	self->carets.resize(1u);
	self->carets.front().ResetSelection();
	self->carets.front().position = changeToRedo->operations[0].start;

	*change = changeToRedo;
	self->historyUndoIndex++;
	
	if (self->ownerEditor)
		self->ownerEditor->StartInsertAnimation();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Comments

static void CommandLineComment(TextController* self, TextChange** outChange) {	
	if (!self->ownerEditor) return;
	if (!self->ownerEditor->language) return;
	if ( self->ownerEditor->language->lineComment.empty()) return;
	
	const Language* language = self->ownerEditor->language;
	
	TextChange* change = *outChange = self->NewTextChange();
	
	for (TextController::Caret& caret : self->carets) {
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			change->ReserveMore((from.line - to.line) + 1);
			
			// find smallest indentation	 of all lines
			// we should look for the visual column in case tabs and spaces are mixed
			// but it's fine for now
			u64 indentation = U64_MAX;	
			for (u64 ln = from.line; ln <= to.line; ln++) {
				const u64 currentIndent = GetIndentationEnd(self, ln);
				if (currentIndent < indentation)
					indentation = currentIndent; 
			}
			
			// now insert the comment prefix
			for (u64 ln = from.line; ln <= to.line; ln++) {			
				self->buffer.InsertInLine(
					TextPosition {
						.line = ln,
						.character = indentation},
					language->lineComment,
					change->NewOperation());
			}
			
			caret.position.character += language->lineComment.size();
			caret.selection.character += language->lineComment.size();
			
		} else {
			self->buffer.InsertInLine(
				TextPosition {
					.line = caret.position.line,
					.character = GetIndentationEnd(self, caret.position.line)},
				language->lineComment,
			change->NewOperation());
			
			caret.position.character += language->lineComment.size();
		}
	}
}

static void CommandUnLineComment(TextController* self, TextChange** change) {
	if (!self->ownerEditor) return;
	if (!self->ownerEditor->language) return;
	if ( self->ownerEditor->language->lineComment.empty()) return;
	
	const Language* language = self->ownerEditor->language;

	for (TextController::Caret& caret : self->carets) {	
		if (TextPosition from, to; caret.GetSelection(&from, &to)) {
			
			for (u64 ln = from.line; ln <= to.line; ln++) {
				const TextBuffer::Line& line = self->buffer.GetLineAt(ln);
				
				const usize pos = line.GetText().find(language->lineComment);
				if (pos == std::string_view::npos) continue;
				
				if (!(*change))
 			 	(*change) = self->NewTextChange();
				
				self->buffer.RemoveInLine(ln, pos, pos + language->lineComment.size(), (*change)->NewOperation());
				
				if (caret.position.line == ln)
					caret.position.character -= language->lineComment.size();
				if (caret.selection.line == ln)
					caret.selection.character -= language->lineComment.size();
			}	
		} else {
			const TextBuffer::Line& line = self->buffer.GetLineAt(caret.position.line);
					
			const usize pos =  line.GetText().find(language->lineComment);
			if (pos == std::string_view::npos) continue;
			if (!(*change))
			 	(*change) = self->NewTextChange();
			self->buffer.RemoveInLine(caret.position.line, pos, pos + language->lineComment.size(), (*change)->NewOperation());
			caret.position.character -= language->lineComment.size();	
		}
	}
}

static void CommandBlockComment(TextController* self, TextChange** outChange) {
	if (!self->ownerEditor) return;
	if (!self->ownerEditor->language) return;
	if  (self->ownerEditor->language->blockComment[0].empty() ||
	     self->ownerEditor->language->blockComment[1].empty()) return;
	
	const Language* language = self->ownerEditor->language;
	
	TextChange* change = *outChange = self->NewTextChange();
	change->ReserveCapacity(self->carets.size() * 2u);
	
	for (TextController::Caret& caret : self->carets) {	
		if (caret.hasSelection) {
			// NOTE: can't use GetSelection() here because we need pointers not copies
	
			TextPosition* start,* end;
			if (caret.position < caret.position) {
				start = &caret.position;
				end   = &caret.selection;
			} else {
				start = &caret.selection;
				end   = &caret.position;
			}		
							
			self->buffer.InsertInLine(*start, language->blockComment[0], change->NewOperation());
			self->buffer.InsertInLine(*end,   language->blockComment[1], change->NewOperation());
			
			if (end->line == start->line)
				end->character += language->blockComment[0].size();
			end->character += language->blockComment[1].size();
		
		} else {
				
			self->buffer.InsertInLine(
				TextPosition {
					.line   = caret.position.line,
					.character = GetIndentationEnd(self, caret.position.line)},
				language->blockComment[0],
			change->NewOperation());
				
			self->buffer.InsertInLine(
				TextPosition {
					.line   = caret.position.line,
					.character = self->buffer.GetLineAt(caret.position.line).length},
				language->blockComment[1],
			change->NewOperation());
			
			caret.position.character += language->blockComment[0].size();
		}
	}
}

static void CommandUnBlockComment(TextController* self, TextChange** change) {
	if (!self->ownerEditor) return;
	if (!self->ownerEditor->language) return;
	if  (self->ownerEditor->language->blockComment[0].empty() ||
	     self->ownerEditor->language->blockComment[1].empty()) return;
	    
	const Language* language = self->ownerEditor->language; 
	
	for (TextController::Caret& caret : self->carets) {	
		if (caret.hasSelection) {
			// NOTE: can't use GetSelection() here because we need pointers not copies
		
			TextPosition* start,* end;
			if (caret.position < caret.position) {
				start = &caret.position;
				end   = &caret.selection;
			} else {
				start = &caret.selection;
				end   = &caret.position;
			}
			
			const TextBuffer::Line& startLine = self->buffer.GetLineAt(start->line);
			const usize posStart = startLine.GetText().find(language->blockComment[0], start->character);
			if (posStart == std::string_view::npos) return;
			
			const TextBuffer::Line& endLine = self->buffer.GetLineAt(end->line);
			const usize posEnd = endLine.GetText().rfind(language->blockComment[1], end->character);
			if (posEnd == std::string_view::npos) return;
			
			if (!(*change)) {
				(*change) = self->NewTextChange();
				(*change)->ReserveMore(2u);
			}
			
			self->buffer.RemoveInLine(end->line,   posEnd,   posEnd   + language->blockComment[1].size(), (*change)->NewOperation());
			self->buffer.RemoveInLine(start->line, posStart, posStart + language->blockComment[0].size(), (*change)->NewOperation());
			
			if (end->line == start->line)
				end->character += language->blockComment[0].size();
			end->character -= language->blockComment[1].size();
			
		} else {
			
			const std::string_view line = self->buffer.GetLineAt(caret.position.line).GetText();
			
			const usize posStart = line.rfind(language->blockComment[0], caret.position.character);
			if (posStart == std::string_view::npos) return;
			
			const usize posEnd = line.find(language->blockComment[1], caret.position.character);
			if (posEnd == std::string_view::npos) return;
			
			if (!(*change)) {
					(*change) = self->NewTextChange();
				(*change)->ReserveMore(2u);
			}
			
			self->buffer.RemoveInLine(caret.position.line, posEnd,   posEnd   + language->blockComment[1].size(), (*change)->NewOperation());
			self->buffer.RemoveInLine(caret.position.line, posStart, posStart + language->blockComment[0].size(), (*change)->NewOperation());
			
			caret.position.character -= language->blockComment[0].size();
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// multi caret

static void CommandAddCaretAbove(TextController* self) {

	TextController::Caret newCaret = self->carets.front();
	if (newCaret.position.line == 0u) return;
				
	MoveLineUp(self, &newCaret.position);
	if (newCaret.hasSelection)
		MoveLineUp(self, &newCaret.selection);
				
	self->carets.insert(self->carets.begin(), newCaret);	
}

static void CommandAddCaretBelow(TextController* self) {
	TextController::Caret newCaret = self->carets.back();
	if (newCaret.position.line == self->buffer.GetMaxLine()) return;
				
	MoveLineDown(self, &newCaret.position);
	if (newCaret.hasSelection)
		MoveLineDown(self, &newCaret.selection);
				
	self->carets.push_back(newCaret);
}

static void CommandClearMultiCarets(TextController* self, bool keepLast) {
	if (keepLast) std::swap(self->carets.front(), self->carets.back());
	self->carets.resize(1u);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Copy and Paste

static void CopyTextToClipboard(std::span<std::string_view> textsToCopy) {
	ASSERT(!textsToCopy.empty());
	
	if (!OpenClipboard(mainWindow.hWnd)) {
		LogError("OpenClipboard() failed. Last Error: %s", StrLastErr(GetLastError()));
		return;
	}	
	DEFER(CloseClipboard());

	EmptyClipboard();
	
	//
	// copy as our own clipboard data
	//
	if (textsToCopy.size() > 1u) {
		
		const UINT cfMultiCaretText = RegisterClipboardFormatA(CLIPBOARD_FORMAT_MULTICARET_TEXT);
		
		const u64 headerMemorySize = sizeof(MultiCaretClipboardData)
			+ (sizeof(MultiCaretClipboardData::CaretData) * (textsToCopy.size() - 1u));
		
		u64 dataMemorySize = 0u;
		for (std::string_view text : textsToCopy)
			dataMemorySize += text.size();
		
		HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, dataMemorySize + headerMemorySize);
		if (!hMemory) {
			LogError("GlobalAlloc() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
	
		auto memory = static_cast<u8*>(GlobalLock(hMemory));
		
		auto multiCaretClipboardData = reinterpret_cast<MultiCaretClipboardData*>(memory);
		multiCaretClipboardData->caretCount = textsToCopy.size();
		
		auto currentDataSegment = reinterpret_cast<char*>(memory + headerMemorySize);
		for (u64  i = 0u; i < textsToCopy.size(); i++) {
			std::string_view text = textsToCopy[i];
			MultiCaretClipboardData::CaretData& caretData = multiCaretClipboardData->data[i];
			
			caretData.length = text.size();
			caretData.data = currentDataSegment;
			
			memcpy(currentDataSegment, text.data(), text.size());
			currentDataSegment += text.size();
		}

		GlobalUnlock(hMemory);		
		SetClipboardData(cfMultiCaretText, hMemory);
	}
	
	//
	// copy as normal text
	//
	{
		u64 totalMemoryRequired = 1u; // +1 for null-terminator
		totalMemoryRequired += 2u * (textsToCopy.size() - 1u); // line breaks
		for (std::string_view text : textsToCopy) // actual texts
			 totalMemoryRequired += text.size();
	
		HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, totalMemoryRequired);
		if (!hMemory) {
			LogError("GlobalAlloc() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
		
		char* mem = static_cast<char*>(GlobalLock(hMemory));	
		
		// first line
		memcpy(mem, textsToCopy.front().data(), textsToCopy.front().size());
		mem += textsToCopy.front().size();
		
		// other line (add linebreak)
		for (u64 i = 1u; i < textsToCopy.size(); i++) {
			const std::string_view text = textsToCopy[i];
			memcpy(mem, text.data(), text.size());
			mem += text.size();
			
			*mem++ = '\r';
			*mem++ = '\n';
		}
		*mem = '\0';
			
		GlobalUnlock(hMemory);
		SetClipboardData(CF_TEXT, hMemory);
	}
}

static void CommandCut(TextController* self, TextChange** outChange) {
	
	std::string_view* textsToCopy = new std::string_view[self->carets.size()];
	DEFER(delete[] textsToCopy);
		
	for (u64 i = 0u; i < self->carets.size(); i++) {
		TextController::Caret& caret = self->carets[i];
		
		TextPosition from, to;
		if (!caret.GetSelection(&from, &to)) {
	
			from = TextPosition {
				.line = caret.position.line,
				.character = GetIndentationEnd(self, caret.position.line) };
	
			to = TextPosition {
				.line = caret.position.line,
				.character = self->buffer.GetLineAt(caret.position.line).length };
		}
		
		TextChange* change = *outChange = self->NewTextChange();
		TextChangeOperation* operation = change->NewOperation();
		
		self->buffer.Remove(from, to, operation);
		
		caret.ResetSelection();
		caret.position = from;
		
		textsToCopy[i] = operation->removedText;
	}
		
	CopyTextToClipboard({textsToCopy, self->carets.size()});
}

static void CommandCopy(TextController* self, TextChange** pchange) {

	std::string_view* textsToCopy = new std::string_view[self->carets.size()];	
	std::string* texts = new std::string[self->carets.size()];
	DEFER(delete[] textsToCopy; delete[] texts;);
	
	for (u64 i = 0u; i < self->carets.size(); i++) {
		const TextController::Caret& caret = self->carets[i];
		
		TextPosition from, to;
		if (!caret.GetSelection(&from, &to)) {
			
			from = TextPosition {
				.line = caret.position.line,
				.character = GetIndentationEnd(self, caret.position.line) };
	
			to = TextPosition {
				.line = caret.position.line,
				.character = self->buffer.GetLineAt(caret.position.line).length };
		}
	
		std::string text = self->buffer.GetText(from, to);	
		texts[i] = std::move(text);
		textsToCopy[i] = texts[i];
	}
		
	CopyTextToClipboard({textsToCopy, self->carets.size()});
}

static void CommandCutLines(TextController* self, TextChange** outChange) {

	std::string_view* textsToCopy = new std::string_view[self->carets.size()];
	DEFER(delete[] textsToCopy);
	
	for (u64 i = 0u; i < self->carets.size(); i++) {
		TextController::Caret& caret = self->carets[i];

		u64 lineFrom, lineTo;
		if (TextPosition selFrom, selTo; caret.GetSelection(&selFrom, &selTo)) {
			lineFrom = selFrom.line;
			lineTo = selTo.line;
	
		} else {
			lineFrom = lineTo = caret.position.line;
		}
		
		TextChange* change = *outChange = self->NewTextChange();
		TextChangeOperation* operation = change->NewOperation();
		
		self->buffer.RemoveChunk(lineFrom, lineTo, operation);
		caret.position = TextPosition {lineFrom, GetIndentationEnd(self, lineFrom)};
		caret.ResetSelection();
		
		textsToCopy[i] = operation->removedText;
	}
		
	CopyTextToClipboard({textsToCopy, self->carets.size()});
}

static void PasteText(TextController* self, u64 caretIndex, std::string_view text, TextChange* change) {
	
	TextController::Caret& caret = self->carets[caretIndex];
	TextChangeOperation* operation = change->NewOperation();
	
	TextPosition insertionPoint {};
	if (TextPosition selectionStart, selectionEnd; caret.GetSelection(&selectionStart, &selectionEnd)) {
		
		self->buffer.Remove(selectionStart, selectionEnd, operation);
		caret.ResetSelection();
		
		insertionPoint = selectionStart;
	
	} else {
		insertionPoint = caret.position;
	}
	
	self->buffer.Insert(insertionPoint, text, operation);
	caret.position = operation->insertionEnd;
	
	AdjustFollowingCarets(self, caretIndex, operation);
	
	if (self->ownerEditor)
		self->ownerEditor->AddInsertAnimationData(operation->start, operation->insertionEnd);
}

static void CommandPaste(TextController* self, TextChange** outChange) {
	
	const UINT cfMultiCaretText = RegisterClipboardFormatA(CLIPBOARD_FORMAT_MULTICARET_TEXT);
	
	if (!IsClipboardFormatAvailable(CF_TEXT) && !IsClipboardFormatAvailable(CF_UNICODETEXT) && !IsClipboardFormatAvailable(cfMultiCaretText)) {
		LogWarning("No clipboard data or clipboard format no compatible");
		return;
	}
		
	if (!OpenClipboard(mainWindow.hWnd)) {
		LogError("OpenClipboard() failed. Last Error: %", StrLastErr(GetLastError()));
		return;
	}
	DEFER(CloseClipboard());
	
	TextChange* change = *outChange = self->NewTextChange();
	
	if (IsClipboardFormatAvailable(cfMultiCaretText) && self->carets.size() > 1u) {
		
		HGLOBAL hMemory = GetClipboardData(cfMultiCaretText);
		if (!hMemory) {
			LogError("GetClipboardData() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
		
		const auto clipboardData = static_cast<const MultiCaretClipboardData*>(GlobalLock(hMemory));
		if (!clipboardData) {
			LogError("GLobalLock() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}		
			
		const u64 limit = std::min(self->carets.size(), clipboardData->caretCount);
		if (self->ownerEditor)
			self->ownerEditor->PrepareInsertAnimation(limit);
		
		for (u64 i = 0u; i < limit; i++) {
			TextController::Caret& caret = self->carets[i];
			const MultiCaretClipboardData::CaretData& caretData = clipboardData->data[i];
			PasteText(self, i, {caretData.data, caretData.length}, change);
		}
		
		GlobalUnlock(hMemory);
	
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		
		HGLOBAL hMemory = GetClipboardData(CF_TEXT);
		if (!hMemory) {
			LogError("GetClipboardData() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
			
		const char* mem = static_cast<const char*>(GlobalLock(hMemory));
		if (!mem) {
			LogError("GLobalLock() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
	
		if (self->ownerEditor)
			self->ownerEditor->PrepareInsertAnimation(self->carets.size());
		
		for (u64 i = 0u; i < self->carets.size(); i++)
			PasteText(self, i, std::string_view {mem}, change);
	
		GlobalUnlock(hMemory);
		
	} else {
		HGLOBAL hMemory = GetClipboardData(CF_UNICODETEXT);
		if (!hMemory) {
			LogError("GetClipboardData() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
			
		const wchar* mem = static_cast<const wchar*>(GlobalLock(hMemory));
		if (!mem) {
			LogError("GLobalLock() failed. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
		DEFER(GlobalUnlock(hMemory));
		
		usize requiredSize = 0u;
		if (!ToUtf8(mem, {}, &requiredSize)) {
			LogError("failed to determine required size for utf-8 conversion. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
		
		std::string utf8 (requiredSize, '\0');
		if (!ToUtf8(mem, utf8, nullptr)) {
			LogError("failed to convert to utf-8. Last Error: %s", StrLastErr(GetLastError()));
			return;
		}
		
		if (self->ownerEditor)
			self->ownerEditor->PrepareInsertAnimation(self->carets.size());
		
		for (u64 i = 0u; i < self->carets.size(); i++)
			PasteText(self, i, utf8, change);
	}
	
	if (self->ownerEditor)
		self->ownerEditor->StartInsertAnimation();
}


static void CommandGotoPrevCaret(TextController* self) {
	for (auto it = self->carets.rbegin(); it != self->carets.rend(); ++it) {
		if (self->editCaretsPosition > it->position) {
			self->editCaretsPosition = it->position;
			return;
		}
	}
}

static void CommandGotoNextCaret(TextController* self) {
	for (const TextController::Caret& caret : self->carets) {
		if (self->editCaretsPosition < caret.position) {
			self->editCaretsPosition = caret.position;
			return;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Input
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static bool HandleEventEditMutiCursor(TextController* self, const Event& event, /*out*/ TextChange** change) {
	
	if (event.type == Event::Type_Text) {
		if (event.text.len == 1u && event.text.data[0] == ' ') {
			self->ToggleCaret();
		} else {
			LeaveEditMultiCaretMode(self);
			InsertText(self, std::string_view {event.text.data, event.text.len}, change);
		}
		return true;
	
	} else if (event.type == Event::Type_KeyPress) {
		if      (event.keypress.vkc == VK_UP     && event.keypress.mods == KM_None) MoveLineUp(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_None) MoveNextChar(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_DOWN   && event.keypress.mods == KM_None) MoveLineDown(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_None) MovePrevChar(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_ESCAPE && event.keypress.mods == KM_None) LeaveEditMultiCaretMode(self);
		else if (event.keypress.vkc == VK_RETURN && event.keypress.mods == KM_None) self->ToggleCaret();
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_Ctrl) MoveToPrevWord(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_Ctrl) MoveToNextWord(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_None) MoveToLineStart(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_None) MoveToLineEnd(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_Ctrl) MoveToBufferStart(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Ctrl) MoveToBufferEnd(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_PRIOR  && event.keypress.mods == KM_None) MovePageUp(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_NEXT   && event.keypress.mods == KM_None) MovePageDown(self, &self->editCaretsPosition);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveNextChar);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_Shift) ShiftCaret(self, MovePrevChar);
		else if (event.keypress.vkc == VK_UP     && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveLineUp);
		else if (event.keypress.vkc == VK_DOWN   && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveLineDown);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == (KM_Ctrl | KM_Shift)) ShiftCaret(self, MoveToNextWord);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == (KM_Ctrl | KM_Shift)) ShiftCaret(self, MoveToPrevWord);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveToLineStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveToLineEnd);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == (KM_Shift | KM_Ctrl)) ShiftCaret(self, MoveToBufferStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Shift) ShiftCaret(self, MoveToBufferEnd);
		else if (event.keypress.vkc == VK_PRIOR  && event.keypress.mods == KM_Shift) ShiftCaret(self, MovePageUp);
		else if (event.keypress.vkc == VK_NEXT   && event.keypress.mods == KM_Shift) ShiftCaret(self, MovePageDown);
		else return false;
		return true;
	
	} else if (event.type == Event::Type_Command) {
		if      (event.cmd.id == Command::Id_Text_MoveCaret)            CommandMoveCaret(self, event.cmd.parameters);
		else if (event.cmd.id == Command::Id_MultiCaret_AddCaretAbove)  CommandGotoPrevCaret(self);
		else if (event.cmd.id == Command::Id_MultiCaret_AddCaretBelow)  CommandGotoNextCaret(self);
		else if (event.cmd.id == Command::Id_MultiCaret_ToggleEditMode) LeaveEditMultiCaretMode(self);
		else return false;
		return true;
	}
	return false;
}

bool TextController::HandleEvent(const Event& event, /*out*/ TextChange** change) {
	ASSERT(!carets.empty());
	
	if (isEditCaretsMode) {
		return HandleEventEditMutiCursor(this, event, change);

	} else if (event.type == Event::Type_Text) {
		InsertText(this, std::string_view {event.text.data, event.text.len}, change);
		return true;
	
	} else if (event.type == Event::Type_KeyPress) {
		if      (event.keypress.vkc == VK_UP     && event.keypress.mods == KM_None)  MoveCaret(this, MoveLineUp);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_None)  MoveCaret(this, MoveNextChar);
		else if (event.keypress.vkc == VK_DOWN   && event.keypress.mods == KM_None)  MoveCaret(this, MoveLineDown);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_None)  MoveCaret(this, MovePrevChar);
		else if (event.keypress.vkc == VK_RETURN && event.keypress.mods == KM_None)  InsertNewLine(this, change);
		else if (event.keypress.vkc == VK_ESCAPE && event.keypress.mods == KM_None)  ClearMultiCarets(this, true);
		else if (event.keypress.vkc == VK_ESCAPE && event.keypress.mods == KM_Ctrl)  ClearMultiCarets(this, false);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_Ctrl)  MoveCaret(this, MoveToPrevWord);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_Ctrl)  MoveCaret(this, MoveToNextWord);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_None)  MoveCaret(this, MoveToLineStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_None)  MoveCaret(this, MoveToLineEnd);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_Ctrl)  MoveCaret(this, MoveToBufferStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Ctrl)  MoveCaret(this, MoveToBufferEnd);
		else if (event.keypress.vkc == VK_PRIOR  && event.keypress.mods == KM_None)  MoveCaret(this, MovePageUp);
		else if (event.keypress.vkc == VK_NEXT   && event.keypress.mods == KM_None)  MoveCaret(this, MovePageDown);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == KM_Shift) Select(this, MoveNextChar);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == KM_Shift) Select(this, MovePrevChar);
		else if (event.keypress.vkc == VK_UP     && event.keypress.mods == KM_Shift) Select(this, MoveLineUp);
		else if (event.keypress.vkc == VK_DOWN   && event.keypress.mods == KM_Shift) Select(this, MoveLineDown);
		else if (event.keypress.vkc == VK_RIGHT  && event.keypress.mods == (KM_Ctrl | KM_Shift)) Select(this, MoveToNextWord);
		else if (event.keypress.vkc == VK_LEFT   && event.keypress.mods == (KM_Ctrl | KM_Shift)) Select(this, MoveToPrevWord);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == KM_Shift) Select(this, MoveToLineStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Shift) Select(this, MoveToLineEnd);
		else if (event.keypress.vkc == VK_HOME   && event.keypress.mods == (KM_Shift | KM_Ctrl)) Select(this, MoveToBufferStart);
		else if (event.keypress.vkc == VK_END    && event.keypress.mods == KM_Shift) Select(this, MoveToBufferEnd);
		else if (event.keypress.vkc == VK_PRIOR  && event.keypress.mods == KM_Shift) Select(this, MovePageUp);
		else if (event.keypress.vkc == VK_NEXT   && event.keypress.mods == KM_Shift) Select(this, MovePageDown);
		else if (event.keypress.vkc == VK_BACK   && event.keypress.mods == KM_None)  Delete(this, change, MovePrevChar);
		else if (event.keypress.vkc == VK_DELETE && event.keypress.mods == KM_None)  Delete(this, change, MoveNextChar);
		else if (event.keypress.vkc == VK_BACK   && event.keypress.mods == KM_Ctrl)  Delete(this, change, MoveToPrevWord);
		else if (event.keypress.vkc == VK_DELETE && event.keypress.mods == KM_Ctrl)  Delete(this, change, MoveToNextWord);
		else return false;
		return true;
	
	} else if (event.type == Event::Type_Command) {
		if      (event.cmd.id == Command::Id_Text_MoveCaret)        CommandMoveCaret(this, event.cmd.parameters);
		else if (event.cmd.id == Command::Id_Text_InsertText)       CommandInsertText(this, event.cmd.parameters, change);
		else if (event.cmd.id == Command::Id_Text_DeleteRange)      CommandDeleteRange(this, event.cmd.parameters, change);
		
		else if (event.cmd.id == Command::Id_Text_SelectAll)        CommandSelectAll(this);
		else if (event.cmd.id == Command::Id_Text_SelectLine)       CommandSelectLine(this);
		else if (event.cmd.id == Command::Id_Text_SelectInBrackets) CommandSelectInBrackets(this);
		else if (event.cmd.id == Command::Id_Text_SelectWord)       CommandSelectWord(this);
		
		else if (event.cmd.id == Command::Id_Text_RepeatText)       CommandRepeatText(this, event.cmd.parameters, change);
		else if (event.cmd.id == Command::Id_Text_TransformCase)    CommandTransformCase(this, event.cmd.parameters, change);
		else if (event.cmd.id == Command::Id_Text_ToUpperCase)      CommandToUpperOrLowerCase(this, change, std::toupper);
		else if (event.cmd.id == Command::Id_Text_ToLowerCase)      CommandToUpperOrLowerCase(this, change, std::tolower);
		
		else if (event.cmd.id == Command::Id_Text_DeleteLine)       CommandDeleteLine(this, change);
		else if (event.cmd.id == Command::Id_Text_IndentLine)       CommandIndentLine(this, change);
		else if (event.cmd.id == Command::Id_Text_UnIndentLine)     CommandUnindentLine(this, change);
		else if (event.cmd.id == Command::Id_Text_CommentLine)      return true; // @TODO
		else if (event.cmd.id == Command::Id_Text_SwapLines)        CommandSwapLines(this, change);
		else if (event.cmd.id == Command::Id_Text_DuplicateLine)    CommandDuplicateLine(this, change);
		else if (event.cmd.id == Command::Id_Text_TrimTrailingWhitespace) CommandTrimTrailingWhitespace(this, change);
		
		else if (event.cmd.id == Command::Id_Text_Undo)             CommandUndo(this, change);
		else if (event.cmd.id == Command::Id_Text_Redo)             CommandRedo(this, change);
		
		else if (event.cmd.id == Command::Id_Clipboard_Cut)         CommandCut(this, change);
		else if (event.cmd.id == Command::Id_Clipboard_Copy)        CommandCopy(this, change);
		else if (event.cmd.id == Command::Id_Clipboard_Paste)       CommandPaste(this, change);
		else if (event.cmd.id == Command::Id_Clipboard_CutLines)    CommandCutLines(this, change);
		
		else if (event.cmd.id == Command::Id_MultiCaret_AddCaretAbove)  CommandAddCaretAbove(this);
		else if (event.cmd.id == Command::Id_MultiCaret_AddCaretBelow)  CommandAddCaretBelow(this);
		else if (event.cmd.id == Command::Id_MultiCaret_ToggleEditMode) EnterEditMultiCaretMode(this, true);
		else return false;
		return true;
	}
	return false;
}
