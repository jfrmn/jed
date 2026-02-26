#include "basic.hh"
#include <iostream>
#include <format>

void _PrintAssertMessage(const char* expression, const char* file, int line, const char* function)
{
	std::cout << std::format("\x1b[41mASSERT\x1b[0m: \"{}\" @ {}:{} {}()", expression, file, line, function) << std::endl;
}

constexpr bool XOR(bool a, bool b) {
	return ((a && !b) || (!a && b));
}