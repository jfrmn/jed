#pragma once
#include <string>
#include <optional>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Check & Require
//
///////////////////////////////////////////////////////////////////////////////////////////////////

enum TestResult {
	 TestResult_Unknown = 0,
	 TestResult_Ok = 1,
	 TestResult_Failed = 2,
	 TestResult_Skipped = 3,
};

template <typename T>
std::optional<std::string> stringify(T value) {
    if      constexpr (requires { std::to_string(value); }) return std::to_string(value);
    else if constexpr (requires { std::string {value}; })   return std::string {value};    
    else return std::nullopt;
}

bool DoCheck(bool passed, const char* left, const char* op, const char* right, std::optional<std::string> leftStr, const char* file, int line, bool required);

// CHECK_XXX macros perform an check and print out an informative message
// if an checks fails the test is set to failure but the execution continues
// check also return a bool indicating whether the check passed or not

#define CHECK_EQ(_a, _b)   { const auto a = _a; DoCheck(a == _b,      #_a, "==", #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_NEQ(_a, _b)  { const auto a = _a; DoCheck(a != _b,      #_a, "!=", #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_LT(_a, _b)   { const auto a = _a; DoCheck(a <  _b,      #_a, "<",  #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_GT(_a, _b)   { const auto a = _a; DoCheck(a >  _b,      #_a, ">",  #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_LTE(_a, _b)  { const auto a = _a; DoCheck(a <= _b,      #_a, "<=", #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_GTE(_a, _b)  { const auto a = _a; DoCheck(a >= _b,      #_a, ">=", #_b,           stringify(a), __FILE__, __LINE__, false); }
#define CHECK_TRUE(_a)     { const auto a = _a; DoCheck(a == true,    #_a, "is", "true",        stringify(a), __FILE__, __LINE__, false); }
#define CHECK_FALSE(_a)    { const auto a = _a; DoCheck(a == false,   #_a, "is", "false",       stringify(a), __FILE__, __LINE__, false); }
#define CHECK_IS_NULL(_a)  { const auto a = _a; DoCheck(a == nullptr, #_a, "is", "nullptr",     stringify(a), __FILE__, __LINE__, false); }
#define CHECK_NOT_NULL(_a) { const auto a = _a; DoCheck(a != nullptr, #_a, "is not", "nullptr", stringify(a), __FILE__, __LINE__, false); }

// REQUIRE_XXX macros perform a check and if that check failes, the test gets aborted
// can be used if some precondition or initialization failed without which the remaining check
// do not make any sense and should not be attempted to run

#define REQUIRE_EQ(_a, _b)   if (const auto a = _a; !DoCheck(a == _b, #_a, "==", #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_NEQ(_a, _b)  if (const auto a = _a; !DoCheck(a != _b, #_a, "!=", #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_LT(_a, _b)   if (const auto a = _a; !DoCheck(a <  _b, #_a, "<",  #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_GT(_a, _b)   if (const auto a = _a; !DoCheck(a >  _b, #_a, ">",  #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_LTE(_a, _b)  if (const auto a = _a; !DoCheck(a <= _b, #_a, "<=", #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_GTE(_a, _b)  if (const auto a = _a; !DoCheck(a >= _b, #_a, ">=", #_b,                stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_TRUE(_a)     if (const auto a = _a; !DoCheck(a == true,    #_a, "is", "true",        stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_FALSE(_a)    if (const auto a = _a; !DoCheck(a == false,   #_a, "is", "false",       stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_IS_NULL(_a)  if (const auto a = _a; !DoCheck(a == nullptr, #_a, "is", "nullptr",     stringify(a), __FILE__, __LINE__, true)) return
#define REQUIRE_NOT_NULL(_a) if (const auto a = _a; !DoCheck(a != nullptr, #_a, "is not", "nullptr", stringify(a), __FILE__, __LINE__, true)) return

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hard setting test results
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void SetTestResult(TestResult testRes, const char* message);

// skip or fail an entire test
// overrides the test result and abort further execution of that test
// prints an optional message (msg may be nullptr)
// SKIP_TEST can be used for broken tests
// FAIL_TEST may be usefull for checks that cannot be expressed with the CHECK_XXX macros
// and SUCCEED_TEST is just here for completness

#define SKIP_TEST(msg) SetTestResult(TestResult_Skipped, msg); return
#define FAIL_TEST(msg) SetTestResult(TestResult_Failed, msg); return
#define SUCCEED_TEST(msg) SetTestResult(TestResult_Ok, msg); return

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Misc
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Event;

// push event to the main window
void PushEvent(const Event& event);
