#pragma once
#include "tests.h"
#include "checks.hh"
#include "logging.hh"
#include "ui/window.hh"

#include <stdarg.h>

// defined in src/main.cc
extern bool Init();
extern void Shutdown();

TestResult testResult = TestResult_Unknown;
va_list testParameters;

int successCount = 0;
int failureCount = 0;
int skipCount = 0;

static void EvaluateTestResult() {
	if (testResult == TestResult_Ok) {
		puts("result: \x1b[42mPASSED\x1b[0m\n");
		successCount++;
	} else if (testResult == TestResult_Failed) {
		puts("result: \x1b[41mFAILED\x1b[0m\n");
		failureCount++;
	} else if (testResult == TestResult_Skipped) {
		puts("result: \x1b[100mSKIPPED\x1b[0m\n");
		skipCount++;
	} else ASSERT_UNREACHABLE;
}

static void RunTest(void(*funcTest)(), const char* funcName) {
	printf("running \x1b[36m%s\x1b[0m\n", funcName);
	testResult = TestResult_Ok;
	funcTest();
	EvaluateTestResult();
	testResult = TestResult_Unknown;
}

static void RunTest(void(*funcTest)(va_list), const char* funcName, const char* paramName, ...) {
	printf("running \x1b[36m%s\x1b[0m with \x1b[34m%s\x1b[0m\n", funcName, paramName);
	testResult = TestResult_Ok;
	va_list args;
	va_start(args, paramName);
	funcTest(args);
	va_end(args);
	EvaluateTestResult();
	testResult = TestResult_Unknown;
}

#define FORWARD_DECLARE_TEST(_func, ...) extern void _func(__VA_OPT__(va_list));
#define RUN_TEST(_func, ...) RunTest(_func, #_func __VA_OPT__(,) #__VA_ARGS__ __VA_OPT__(,) __VA_ARGS__);

X_TESTS(FORWARD_DECLARE_TEST)

int main(int argc, char** argv) {
	OpenLogger(LogLevel_Trace, LogOutput_Temporary);		
	CHECK_TRUE(Init());
	mainWindow.Show();
	
	X_TESTS(RUN_TEST);
			
	Shutdown();
	CloseLogger();
	printf("\nPassed: \x1b[32m%d\x1b[0m  Failed: \x1b[31m%d\x1b[0m  Skipped: \x1b[90m%d\x1b[0m\n", successCount, failureCount, skipCount);
	return failureCount > 0 ? -1 : 0;
}
