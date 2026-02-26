#pragma once

//------------------------------------------------------------------------------
// Typedefs
//------------------------------------------------------------------------------

using u8     = unsigned __int8;
using s8     = signed   __int8;
using u16    = unsigned __int16;
using s16    = signed   __int16;
using u32    = unsigned __int32;
using s32    = signed   __int32;
using u64    = unsigned __int64;
using s64    = signed   __int64;
using usize  = size_t;
using f32    = float;
using f64    = double;
using wchar  = wchar_t;
using char8  = char;
using char16 = wchar_t;
using char32 = char32_t;

//------------------------------------------------------------------------------
// Numeric Limits
//------------------------------------------------------------------------------

#define U8_MAX  255u
#define S8_MAX  127
#define S8_MIN -128

#define U16_MAX  65535u
#define S16_MAX  32767
#define S16_MIN -32768

#define U16_MAX  65535u
#define S16_MAX  32767
#define S16_MIN -32768

#define U32_MAX  4294967295u
#define S32_MAX  2147483647
#define S32_MIN -2147483648

#define USIZE_MAX 18'446'744'073'709'551'615u
#define U64_MAX   18'446'744'073'709'551'615u
#define S64_MAX   9'223'372'036'854'775'807
#define S64_MIN  (-9'223'372'036'854'775'807ll - 1ll)

#define F32_PI 3.14159265f

//-----------------------------------------------------------------------------
// Asserts
//-----------------------------------------------------------------------------

// get the debug break function without including windows.h
extern "C" __declspec(dllimport) void __stdcall DebugBreak(void);

#define ASSERT(expr) if (!(expr)) { _PrintAssertMessage(#expr, __FILE__, __LINE__, __FUNCTION__); DebugBreak(); }
#define ASSERT_UNREACHABLE { _PrintAssertMessage("Unreachable code hit", __FILE__, __LINE__, __FUNCTION__); DebugBreak(); }
#define ASSERT_NOT_IMPLEMENTED { _PrintAssertMessage("Reached a code path that is not implemented yet", __FILE__, __LINE__, __FUNCTION__); }

void _PrintAssertMessage(const char* expression, const char* file, int line, const char* function);


//------------------------------------------------------------------------------
// Wrap arounds
//------------------------------------------------------------------------------

// Increments/Decrements a number and wraps it around limit, so that it will become 0
// and around 0 so that it will become limit-1 in that case
// The increment variant is a well-known trick
// for the decrement variant see here:
//   https://stackoverflow.com/a/39740009
//   slightly modified to that it workds with unsigned ints as well

template<class T>
T IncrementWrapAround(T number, T limit) {
	return (number + 1) % limit;
}

template<class T>
T DecrementWrapAround(T number, T limit) {
	return (number + (limit - 1)) % limit;
}

//------------------------------------------------------------------------------
// Defer
//------------------------------------------------------------------------------

template<class T>
struct Deferer {
	T func;
	~Deferer() noexcept { func(); };
};

#define _MAKE_DEFERER_NAME_2(ln) defererAtLine ## ln
#define _MAKE_DEFERER_NAME(ln) _MAKE_DEFERER_NAME_2(ln)

#define DEFER(statement) Deferer _MAKE_DEFERER_NAME(__LINE__) { .func = [&]() { statement; } }

//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

#define STATIC_ARRAY_SIZE(staticArray) sizeof(staticArray) / sizeof(staticArray[0])

extern constexpr bool XOR(bool a, bool b);

#define function_cast(signature, func) reinterpret_cast<signature>(reinterpret_cast<intptr_t>(func))

#define DISALLOW_COPY_AND_ASSING(clazz)\
	clazz() = default;\
	clazz(const clazz&) = delete;\
	clazz& operator=(const clazz&) = delete;\
	~clazz() noexcept;