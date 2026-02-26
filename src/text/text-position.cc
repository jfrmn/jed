#include "text-position.hh"

bool TextPosition::operator<(const TextPosition& other) const {
	
	if (line < other.line)
		return true;
	if (line > other.line)
		return false;
	
	return column < other.column;
}

bool TextPosition::operator<=(const TextPosition& other) const {
	
	if (line < other.line)
		return true;
	if (line > other.line)
		return false;
	
	return column <= other.column;
}

bool TextPosition::operator>(const TextPosition& other) const {
	
	if (line > other.line)
		return true;
	if (line < other.line)
		return false;
	
	return column > other.column;
}

bool TextPosition::operator>=(const TextPosition& other) const {
	
	if (line > other.line)
		return true;
	if (line < other.line)
		return false;
	
	return column >= other.column;
}

bool TextPosition::operator==(const TextPosition& other) const {
	return this->line == other.line && this->column == other.column;
}
