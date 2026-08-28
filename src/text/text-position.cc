#include "text-position.hh"

bool TextPosition::operator<(const TextPosition& other) const {
	
	if (line < other.line)
		return true;
	if (line > other.line)
		return false;
	
	return character < other.character;
}

bool TextPosition::operator<=(const TextPosition& other) const {
	
	if (line < other.line)
		return true;
	if (line > other.line)
		return false;
	
	return character <= other.character;
}

bool TextPosition::operator>(const TextPosition& other) const {
	
	if (line > other.line)
		return true;
	if (line < other.line)
		return false;
	
	return character > other.character;
}

bool TextPosition::operator>=(const TextPosition& other) const {
	
	if (line > other.line)
		return true;
	if (line < other.line)
		return false;
	
	return character >= other.character;
}

bool TextPosition::operator==(const TextPosition& other) const {
	return this->line == other.line && this->character == other.character;
}
