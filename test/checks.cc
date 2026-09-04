#include "checks.hh"
#include "app.hh"
#include "events.hh"
#include "logging.hh"

#include <stdio.h>

extern TestResult testResult;

bool DoCheck(bool passed, const char* left, const char* op, const char* right, std::optional<std::string> leftStr, const char* file, int line, bool required) {
	printf(" \x1b[%dm%s\x1b[0m | \x1b[97m%s \x1b[0m%s \x1b[95m%s\x1b[0m\n", passed ? 32 : 31, passed ? "PASS" : "FAIL", left, op, right);
	if (!passed) {
		testResult = TestResult_Failed;
		if (leftStr.has_value()) printf("      | \x1b[31m%s\x1b[0m = %.*s\n", left, SIZE_AND_DATA(leftStr.value()));
		if (required) puts("      | \x1b[31mskipping remaining checks\x1b[0m");
		printf("      | %s:%d\n", file, line);
	}
	return passed;
}

void SetTestResult(TestResult testRes, const char* message) {
	int color = 0;
	const char* label = nullptr;
	if (testRes == TestResult_Ok) {
		if (!message) message = "Test set to success";
		color = 32;
		label = "PASS";
		
	} else if (testRes == TestResult_Failed) {
		if (!message) message = "Test set to failure";
		color = 31;
		label = "FAIL";
	} else {
		if (!message) message = "Test skipped";
		color = 0;
		label = "SKIP";
	}
	
	printf(" \x1b[%dm%s\x1b[0m | %s\n", color, label, message);
	testResult = testRes;
}

void PushEvent(const Event& event) {
	app.HandleEvent(event);
	app.Update();
	mouse.NextFrame(event);
}
