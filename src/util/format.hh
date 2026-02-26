#pragma once
#include "basic.hh"
#include <ostream>
#include <string>
#include <span>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct FormatArgument {
	const void* userdata = nullptr;
	void (*Write) (const void* userdata, /*inout*/ std::ostream* traget) = nullptr;
};

void FormatValue(const std::wstring* val, std::ostream* sink);
void FormatValue(const std::wstring_view* val, std::ostream* sink);
void FormatValue(const wchar* const* val, std::ostream* sink);
void FormatValue(const wchar* val, std::ostream* sink);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<class T>
concept HasLeftShiftOperator = requires(T t, std::ostream o) { o << t; };

template<class T> requires HasLeftShiftOperator<T>
void FormatValue(const T* val, std::ostream* sink) {
	(*sink) << (*val);
}

template<class T>
concept HasFormatValueOverload = requires(T * t, std::ostream * o) { FormatValue(t, o); };

template<class T>
FormatArgument MakeFormatArg(const T* val) {
	static_assert(false, "FormatValue() function for the specified type not found");
}

template<class T> requires HasFormatValueOverload<T>
FormatArgument MakeFormatArg(const T* val) {
	void (*FormatValueFunc)(const T*, std::ostream*) = FormatValue;
	return FormatArgument {
		.userdata = val,
		.Write = reinterpret_cast<void(*)(const void*, std::ostream*)>(FormatValueFunc) };
}

template<>
inline FormatArgument MakeFormatArg<>(const FormatArgument* fmtArg) {
	return *fmtArg;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
consteval bool CheckArgs(std::string_view fmt, u64 providedArgs) {
	size_t cnt = 0;
	for (size_t i = 0; i < fmt.size(); i++) {
		if (fmt[i] == '%') cnt += 1;
	}

	return cnt == providedArgs;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<bool ok, class...Args>
void FormatChecked(std::ostream* sink, std::string_view fmt, Args&&...args) {
	static_assert(ok, "Provided arguments != arguments required by the format string");
	const FormatArgument arguments[] {MakeFormatArg(&args)...};
	FormatWithArgs(sink, fmt, std::span {arguments});
}

template<bool ok, class...Args>
std::string FormatStringChecked(std::string_view fmt, Args&&...args) {
	static_assert(ok, "Provided arguments != arguments required by the format string");
	const FormatArgument arguments[] {MakeFormatArg(&args)...};
	return FormatStringWithArgs(fmt, std::span {arguments});
}

template<bool ok, class...Args>
void FormatToStringChecked(std::string* sink, std::string_view fmt, Args&&...args) {
	static_assert(ok, "Provided arguments != arguments required by the format string");
	const FormatArgument arguments[] {MakeFormatArg(&args)...};
	FormatToStringWithArgs(sink, fmt, std::span {arguments});
}

template<bool ok, class...Args>
u64 FormatToBufferChecked(std::span<char> buffer, std::string_view fmt, Args&&...args) {
	static_assert(ok, "Provided arguments != arguments required by the format string");
	const FormatArgument arguments[] {MakeFormatArg(&args)...};
	return FormatToBufferWithArgs(buffer, fmt, std::span {arguments});
}

void        FormatWithArgs(std::ostream* sink, std::string_view fmt, std::span<const FormatArgument> arguments);
std::string FormatStringWithArgs(std::string_view fmt, std::span<const FormatArgument> arguments);
void        FormatToStringWithArgs(std::string* sink, std::string_view fmt, std::span<const FormatArgument> arguments);
u64         FormatToBufferWithArgs(std::span<char> buffer, std::string_view fmt, std::span<const FormatArgument> arguments);


#define SELECT_NUMBER(                                            _1, _2, _3, _4, _5, _6, _7, _N, ...) _N
#define COUNT_VA_ARGS(...) SELECT_NUMBER(__VA_ARGS__ __VA_OPT__(,) 7,  6,  5,  4,  3,  2,  1,  0)

#define Format(sink, fmt, ...)\
	FormatChecked<\
		CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>\
		(sink, fmt __VA_OPT__(,) __VA_ARGS__)

#define FormatString(fmt, ...)\
	FormatStringChecked<\
		CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>\
		(fmt __VA_OPT__(,) __VA_ARGS__)

#define FormatToString(sink, fmt, ...)\
	FormatToStringChecked<\
		CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>\
		(sink, fmt __VA_OPT__(,) __VA_ARGS__)

#define FormatToBuffer(buffer, fmt, ...)\
	FormatToBufferChecked<\
		CheckArgs(fmt, COUNT_VA_ARGS(__VA_ARGS__))>\
		(buffer, fmt __VA_OPT__(,) __VA_ARGS__)
