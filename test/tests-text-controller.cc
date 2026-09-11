#include "checks.hh"
#include "events.hh"
#include "app.hh"
#include "commands.hh"
#include "editor/editor.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

void Test_TextController_Movements() {
	Editor* editor = nullptr;
	REQUIRE_INIT_EDITOR("The quick brown\n"
                        "fox jumps over the\n"
                        "lazy dog!", &editor);
	
	TextController& textController = app.tabs.front().editor->textController;
	
	auto Move = [&](u32 key, u32 mods, TextPosition start, TextPosition expected) {
		textController.SetCaretPosition(start);
		editor->scrollarea.vpY = 0.0f;
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
		editor->scrollarea.vpY = 0.0f;
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
	
	REQUIRE_CLOSE_EDITOR();
}

void Test_TextController_Commands() {
	Editor* editor = nullptr;
	REQUIRE_INIT_EDITOR({}, &editor);

	TextController& textController = editor->textController;

	auto RunCommand = [&](Command::Id id, std::vector<ParameterValue> parameters = {}) {
		PushEvent(Event {
			.type = Event::Type_Command,
			.cmd = Command {.id = id, .parameters = parameters}});
	};
	auto Text = [&]() {
		return textController.buffer.GetText(
			TextPosition {0u, 0u},
			TextPosition {
				textController.buffer.GetMaxLine(),
				textController.buffer.lines.back().length});
	};

	SetEditorText(editor, "the quick brown\nfox jumps over the\nlazy dog!");
	RunCommand(Command::Id_Text_MoveCaret, {
		ParameterValue {.numberValue = 1},
		ParameterValue {.numberValue = 4}});
	CHECK_EQ(textController.carets.front().position.line, 1u);
	CHECK_EQ(textController.carets.front().position.character, 4u);

	RunCommand(Command::Id_Text_InsertText, {ParameterValue {.stringValue = "!"}});
	CHECK_EQ(Text(), "the quick brown\nfox !jumps over the\nlazy dog!");

	RunCommand(Command::Id_Text_SelectAll);
	CHECK_EQ(textController.carets.front().selection.line, 0u);
	CHECK_EQ(textController.carets.front().selection.character, 0u);
	CHECK_EQ(textController.carets.front().position.line, 2u);
	CHECK_EQ(textController.carets.front().position.character, 9u);
	CHECK_TRUE(textController.carets.front().hasSelection);

	SetEditorText(editor, "the quick brown\nfox jumps over the\nlazy dog!");
	textController.SetCaretPosition({0u, 6u});
	RunCommand(Command::Id_Text_SelectLine);
	CHECK_EQ(textController.carets.front().selection.line, 0u);
	CHECK_EQ(textController.carets.front().selection.character, 0u);
	CHECK_EQ(textController.carets.front().position.line, 0u);
	CHECK_EQ(textController.carets.front().position.character, 15u);

	SetEditorText(editor, "call(foo + bar)");
	textController.SetCaretPosition({0u, 7u});
	RunCommand(Command::Id_Text_SelectInBrackets);
	CHECK_EQ(textController.carets.front().selection.line, 0u);
	CHECK_EQ(textController.carets.front().selection.character, 5u);
	CHECK_EQ(textController.carets.front().position.line, 0u);
	CHECK_EQ(textController.carets.front().position.character, 14u);

	SetEditorText(editor, "the quick brown");
	textController.SetCaretPosition({0u, 6u});
	RunCommand(Command::Id_Text_SelectWord);
	CHECK_EQ(textController.carets.front().selection.line, 0u);
	CHECK_EQ(textController.carets.front().selection.character, 4u);
	CHECK_EQ(textController.carets.front().position.line, 0u);
	CHECK_EQ(textController.carets.front().position.character, 9u);

	SetEditorText(editor, "x");
	textController.SetCaretPosition({0u, 1u});
	RunCommand(Command::Id_Text_RepeatText, {
		ParameterValue {.stringValue = "ab"},
		ParameterValue {.numberValue = 3}});
	CHECK_EQ(Text(), "xababab");

	SetEditorText(editor, "Hello");
	textController.SetSelection({0u, 0u}, {0u, 5u});
	RunCommand(Command::Id_Text_ToLowerCase);
	CHECK_EQ(Text(), "hello");
	textController.SetSelection({0u, 0u}, {0u, 5u});
	RunCommand(Command::Id_Text_ToUpperCase);
	CHECK_EQ(Text(), "HELLO");

	SetEditorText(editor, "first\nsecond\nthird");
	textController.SetCaretPosition({1u, 2u});
	RunCommand(Command::Id_Text_DeleteLine);
	CHECK_EQ(Text(), "first\nthird");

	SetEditorText(editor, "first\nsecond");
	textController.SetCaretPosition({1u, 3u});
	RunCommand(Command::Id_Text_IndentLine);
	CHECK_EQ(Text(), "first\n\tsecond");
	RunCommand(Command::Id_Text_UnIndentLine);
	CHECK_EQ(Text(), "first\nsecond");

	SetEditorText(editor, "first\nsecond");
	textController.SetCaretPosition({0u, 2u});
	RunCommand(Command::Id_Text_DuplicateLine);
	CHECK_EQ(Text(), "first\nfirst\nsecond");

	RunCommand(Command::Id_Text_Undo);
	CHECK_EQ(Text(), "first\nsecond");
	RunCommand(Command::Id_Text_Redo);
	CHECK_EQ(Text(), "first\nfirst\nsecond");

	SetEditorText(editor, "first\nsecond\nthird");
	textController.SetCaretPosition({1u, 2u});
	RunCommand(Command::Id_MultiCaret_AddCaretAbove);
	CHECK_EQ(textController.carets.size(), 2u);
	CHECK_EQ(textController.carets.front().position.line, 0u);
	CHECK_EQ(textController.carets.front().position.character, 2u);
	RunCommand(Command::Id_MultiCaret_AddCaretBelow);
	CHECK_EQ(textController.carets.size(), 3u);

	RunCommand(Command::Id_MultiCaret_ToggleEditMode);
	CHECK_TRUE(textController.isEditCaretsMode);
	
	REQUIRE_CLOSE_EDITOR();
}
