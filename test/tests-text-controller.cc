#include "checks.hh"
#include "events.hh"
#include "app.hh"
#include "settings.hh"
#include "editor/editor.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

static bool InitEditor(std::string_view title) {
	Editor* editor = new Editor();
	App::Tab& tab = app.tabs.emplace_back();
	tab.editor = editor;
	tab.editor->Init();
	tab.panelIndex = 0u;
	tab.title.Shape(title, settings.fontUi);
	
	App::Panel& panel = app.panels.emplace_back();
	panel.editor = tab.editor;
	panel.tabIndex = 0u;
	
	app.focusedPanelIndex = 0u;
	
	std::string* string = tab.editor->textController.buffer.Clear();
	*string =
		"the quick brown\n"
		"fox jumps over the\n"
		"lazy dog!";
	
	tab.editor->textController.buffer.RecreateLines();
	tab.editor->textController.Reset();
	
	editor->glyphRuns.resize(editor->textController.buffer.LineCount());
	return GlyphRun::ShapeBatch(editor->textController.buffer, settings.fontEditor, editor->glyphRuns);
};

void Test_TextController_Movements() {
	REQUIRE_TRUE(InitEditor("Movements"));
	
	TextController& textController = app.tabs.front().editor->textController;
	
	PushEvent(Event {
		.type = Event::Type_KeyPress,
		.keypress = VK_RIGHT});
	CHECK_EQ(textController.carets.front().position.line, 0);
	CHECK_EQ(textController.carets.front().position.character, 1);
	CHECK_EQ(textController.carets.size(), 1u);
	
	PushEvent(Event {
		.type = Event::Type_KeyPress,
		.keypress = {VK_RIGHT, KM_Ctrl}});
	CHECK_EQ(textController.carets.front().position.line, 0);
	CHECK_EQ(textController.carets.front().position.character, 3);
	CHECK_EQ(textController.carets.size(), 1u);
}
