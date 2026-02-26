#include "format.hh"
#include <sstream>
#include <spanstream>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FormatValue(const std::wstring* val, std::ostream* sink) {
#pragma warning (disable : 4244) // possible loss of data
	const std::string str(val->begin(), val->end());
	(*sink) << str;
}

void FormatValue(const std::wstring_view* val, std::ostream* sink) {
	const std::string str(val->begin(), val->end());
	(*sink) << str;
}

void FormatValue(const wchar* const* val, std::ostream* sink) {
	const std::string str(*val, *val + wcslen(*val));
	(*sink) << str;
}

void FormatValue(const wchar* val, std::ostream* sink) {
	(*sink) << static_cast<char>(*val);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void FormatWithArgs(std::ostream* sink, std::string_view fmt, std::span<const FormatArgument> arguments) {
	
	u64 argIndex = 0u;
	u64 chunkStart = 0u;
	for (u64 i = 0u; i < fmt.size(); i++) {
		
		if (fmt[i] == '%') {
			const u64 chunkSize = i - chunkStart;
			sink->write(fmt.data() + chunkStart, chunkSize);
			
			if (argIndex < arguments.size()) {
				const FormatArgument* currentArg = &arguments[argIndex];
				currentArg->Write(currentArg->userdata, sink);
				argIndex += 1;
			
			} else {
				(*sink) << "%MISSING ARG%";
			}
			
			chunkStart = i + 1;
		}
	}
	
	// print rest of fmt
	if (chunkStart < fmt.size()) {
		const u64 chunkSize = fmt.size() - chunkStart;
		sink->write(fmt.data() + chunkStart, chunkSize);
	}
	
	// print leftover args
	if (argIndex < arguments.size()) {
		(*sink) << "%ARGS LEFTOVER: " << (arguments.size() - argIndex) << " [ ";
		for (u64 i = argIndex; i < arguments.size(); i++) {
			const FormatArgument* currentArg = &arguments[i];
			currentArg->Write(currentArg->userdata, sink);
			sink->write(" ", 1);
		}
		(*sink) << "]%";
	}
}

std::string FormatStringWithArgs(std::string_view fmt, std::span<const FormatArgument> arguments) {
	std::ostringstream strstream {};
	FormatWithArgs(&strstream, fmt, arguments);
	return strstream.str();
}

void FormatToStringWithArgs(std::string* sink, std::string_view fmt, std::span<const FormatArgument> arguments) {
	std::ostringstream strstream {std::move(*sink)};
	FormatWithArgs(&strstream, fmt, arguments);
	*sink = std::move(strstream.str());
}

u64 FormatToBufferWithArgs(std::span<char> target, std::string_view fmt, std::span<const FormatArgument> arguments) {
	std::ospanstream spanstream {target};
	FormatWithArgs(&spanstream, fmt, arguments);
	
	const auto pos = spanstream.tellp();
	ASSERT(pos >= 0)
	return static_cast<u64>(pos);
}
