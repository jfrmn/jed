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
	Editor& editor = *app.tabs.front().editor;
	
	auto Move = [&](u32 key, u32 mods, TextPosition start, TextPosition expected) {
		textController.SetCaretPosition(start);
		editor.scrollarea.vpY = 0.0f;
		PushEvent(Event {
			.type = Event::Type_KeyPress,
			.keypress = {key, mods}});
		CHECK_EQ(textController.carets.front().position.line, expected.line);
		CHECK_EQ(textController.carets.front().position.character, expected.character);
		CHECK_EQ(textController.carets.size(), 1u);
		CHECK_FALSE(textController.carets.front().hasSelection);
	};

	Move(VK_RIGHT, KM_None, {0u, 0u}, {0u, 1u});
	Move(VK_LEFT, KM_None, {0u, 1u}, {0u, 0u});
	Move(VK_DOWN, KM_None, {0u, 5u}, {1u, 5u});
	Move(VK_UP, KM_None, {1u, 5u}, {0u, 5u});
	Move(VK_RIGHT, KM_Ctrl, {0u, 1u}, {0u, 3u});
	Move(VK_LEFT, KM_Ctrl, {0u, 14u}, {0u, 10u});
	Move(VK_HOME, KM_None, {0u, 8u}, {0u, 0u});
	Move(VK_END, KM_None, {0u, 8u}, {0u, 15u});
	Move(VK_HOME, KM_Ctrl, {1u, 8u}, {0u, 0u});
	Move(VK_END, KM_Ctrl, {0u, 8u}, {2u, 9u});
	Move(VK_PRIOR, KM_None, {1u, 8u}, {0u, 0u});
	Move(VK_NEXT, KM_None, {0u, 8u}, {2u, 9u});

	auto Select = [&](u32 key, u32 mods, TextPosition start, TextPosition expected) {
		textController.SetCaretPosition(start);
		editor.scrollarea.vpY = 0.0f;
		PushEvent(Event {
			.type = Event::Type_KeyPress,
			.keypress = {key, mods}});
		const TextController::Caret& caret = textController.carets.front();
		CHECK_EQ(caret.position.line, expected.line);
		CHECK_EQ(caret.position.character, expected.character);
		CHECK_EQ(caret.selection.line, start.line);
		CHECK_EQ(caret.selection.character, start.character);
		CHECK_TRUE(caret.hasSelection);
	};

	Select(VK_RIGHT, KM_Shift, {0u, 0u}, {0u, 1u});
	Select(VK_LEFT, KM_Shift, {0u, 1u}, {0u, 0u});
	Select(VK_DOWN, KM_Shift, {0u, 5u}, {1u, 5u});
	Select(VK_UP, KM_Shift, {1u, 5u}, {0u, 5u});
	Select(VK_RIGHT, KM_Ctrl | KM_Shift, {0u, 1u}, {0u, 3u});
	Select(VK_LEFT, KM_Ctrl | KM_Shift, {0u, 14u}, {0u, 10u});
	Select(VK_HOME, KM_Shift, {0u, 8u}, {0u, 0u});
	Select(VK_END, KM_Shift, {0u, 8u}, {0u, 15u});
	Select(VK_HOME, KM_Ctrl | KM_Shift, {1u, 8u}, {0u, 0u});
	Select(VK_END, KM_Ctrl | KM_Shift, {0u, 8u}, {2u, 9u});
	Select(VK_PRIOR, KM_Shift, {1u, 8u}, {0u, 0u});
	Select(VK_NEXT, KM_Shift, {0u, 8u}, {2u, 9u});
}
