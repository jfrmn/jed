#include "checks.hh"
#include "app.hh"
#include "events.hh"
#include "logging.hh"
#include "settings.hh"
#include "editor/editor.hh"

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

bool InitEditor(std::string_view title, std::string_view text, /*out*/ Editor** editor) {
	Editor* newEditor = *editor = new Editor();
	App::Tab& tab = app.tabs.emplace_back();
	tab.editor = newEditor;
	tab.editor->Init();
	tab.panelIndex = 0u;
	tab.title.Shape(title, settings.fontUi);
	
	App::Panel& panel = app.panels.emplace_back();
	panel.editor = tab.editor;
	panel.tabIndex = app.tabs.size() - 1u;
	
	app.focusedPanelIndex = app.panels.size() - 1u;
	
	std::string* string = tab.editor->textController.buffer.Clear();
	*string = text;
	
	tab.editor->textController.buffer.RecreateLines();
	tab.editor->textController.Reset();
	
	newEditor->glyphRuns.resize(newEditor->textController.buffer.LineCount());
	return GlyphRun::ShapeBatch(newEditor->textController.buffer, settings.fontEditor, newEditor->glyphRuns);	
};

bool CloseEditor() {
	if (app.tabs.empty()) return false;
	if (app.panels.empty()) return false;
	
	App::Tab& tab = app.tabs.back();
	delete tab.editor;
	app.tabs.pop_back();
	app.panels.pop_back();
	app.focusedPanelIndex = U64_MAX;
	return true;
}

void SetEditorText(Editor* editor, std::string_view text) {
	std::string* string = editor->textController.buffer.Clear();
	*string = text;
	editor->textController.buffer.RecreateLines();
	editor->textController.Reset();
	editor->glyphRuns.resize(editor->textController.buffer.LineCount());
	REQUIRE_TRUE(GlyphRun::ShapeBatch(editor->textController.buffer, settings.fontEditor, editor->glyphRuns));
}

void PushEvent(const Event& event) {
	app.HandleEvent(event);
	app.Update();
	mouse.NextFrame(event);
}

void Update() {
	app.Update();
	mouse.NextFrame(Event {.type = Event::Type_None});
}
