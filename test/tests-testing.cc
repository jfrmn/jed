#include "checks.hh"
#include "basic.hh"
#include <stdarg.h>

void Test_Testing_Checks() {
	const int foo = 1;
	CHECK_EQ(foo, 1);
	CHECK_NEQ(foo, 2);
	CHECK_LT(foo, 3);
	CHECK_GT(foo, 0);
	CHECK_LTE(foo, 2);	
	CHECK_GTE(foo, 0);
	
	const bool iAmTrue = true;
	CHECK_TRUE(iAmTrue);
	
	const bool iAmFalse = false;
	CHECK_FALSE(iAmFalse);
	
	const int* nullPtr = nullptr;
	CHECK_IS_NULL(nullPtr);
	
	const int* notNullPtr = &foo;
	CHECK_NOT_NULL(notNullPtr);
}

void Test_Testing_TestWithParameter(va_list params) {
	 const int parameterA = va_arg(params, int);
	 const char* parameterB = va_arg(params, const char*);
	CHECK_EQ(parameterA, 123);
	
	const std::string_view someString = "Hello Tests!";
	CHECK_EQ(someString, parameterB);
}

void Test_Testing_Skipping() {
	SUCCEED_TEST("this test should always be skipped");
	ASSERT_UNREACHABLE;
}
