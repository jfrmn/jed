#include "commands.hh"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static ParameterDefinition editorOpenSearchBarParameter {
	.type = ParameterDefinition::Type_Bool,
	.name = "open in replace mode",
	.defaultValue = ParameterValue {.boolValue = false}};

static ParameterDefinition editorScrollParameter {
	.type = ParameterDefinition::Type_Number,
	.name = "amount to scroll in lines",
	.defaultValue = ParameterValue {.numberValue = 2},
	.minValue = 0};

static ParameterDefinition insertTextParameter {
	.type = ParameterDefinition::Type_String,
	.name = "text to insert"};

static ParameterDefinition moveCaretParameter[] {
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "line of the target position (0-based)",
		.minValue = 0u},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "column of the target position (0-based)",
		.minValue = 0u}};

static ParameterDefinition deleteRangeParameter[] {
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "line of start position (0-based)",
		.minValue = 0u},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "column of start position (0-based)",
		.minValue = 0u},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "line of end position (0-based)",
		.minValue = 0u},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "column of end position (0-based)",
		.minValue = 0u}};

static ParameterDefinition repeatTextParameters[] {
	ParameterDefinition {
		.type = ParameterDefinition::Type_String,
		.name = "text to insert"},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Number,
		.name = "repeats"}};

static ParameterDefinition commentLineParameter {
	.type = ParameterDefinition::Type_Number,
	.name = "index of the comment style if the language supports multiple styles",
	.minValue = 0u};

static ParameterDefinition transformCaseParameter[] {
	ParameterDefinition {
		.type = ParameterDefinition::Type_Enum,
		.name = "target case",
		.enumValues = {
			ParameterDefinition::EnumValue {.name = "camelCase"},
			ParameterDefinition::EnumValue {.name = "snake_case"},
			ParameterDefinition::EnumValue {.name = "kebab-case"},
			ParameterDefinition::EnumValue {.name = "space case"}}},
	ParameterDefinition {
		.type = ParameterDefinition::Type_Bool,
		.name = "capitalize first letters (e.g. snake_case -> Snake_Case)",
		.defaultValue = ParameterValue {.boolValue = false}}};

static ParameterDefinition insertNumbersParameter {
	.type = ParameterDefinition::Type_Number,
	.name = "the starting number"};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
CommandDefinition commandDefinitions[] {
	CommandDefinition {
		.name = "noop",
		.description = "does nothing"},
	CommandDefinition {
		.name = "open-file-search",
		.description = "opens the file search bar"},
	CommandDefinition {
		.name = "open-tool-search",
		.description = "opens the tool search bar"},
	CommandDefinition {
		.name = "open-command-search",
		.description = "opens the command search bar"},
	CommandDefinition {
		.name = "toggle-tool-output",
		.description = "toggles the tool output panel"},
	CommandDefinition {
		.name = "toggle-explorer",
		.description = "toggles the built-in explorer"},
	CommandDefinition {
		.name = "focus-next-tab",
		.description = "focuses the next tab"},
	CommandDefinition {
		.name = "focus-prev-tab",
		.description = "focuses the previous tab"},
	CommandDefinition {
		.name = "focus-next-panel",
		.description = "focuses the next panel"},
	CommandDefinition {	
		.name = "focus-prev-panel",
		.description = "focuses the previous panel"},
	CommandDefinition {	
		.name = "swap-panels",
		.description = "swaps the current panel with the next panel"},
	CommandDefinition {	
		.name = "close-panel",
		.description = "closes the current panel"},
	CommandDefinition {
		.name = "add-panel-after",
		.description = "adds a new panel after the current panel"},
	CommandDefinition {
		.name = "add-panel-before",
		.description = "adds a new panel before the current panel"},
	CommandDefinition {	
		.name = "new-file",
		.description = "creates a new file"},
	CommandDefinition {	
		.name = "close-file",
		.description = "closes the current file"},
	CommandDefinition {	
		.name = "close-files-to-the-right",
		.description = "closes all files to the right of the current file"},
	CommandDefinition {	
		.name = "close-files-to-the-left",
		.description = "closes all files to the left of the current file"},
	CommandDefinition {	
		.name = "close-other-files",
		.description = "closes all files except the current file"},
	CommandDefinition {		
		.name = "close-panel-and-file",
		.description = "closes the current panel and file"},
	CommandDefinition {	
		.name = "save-all",
		.description = "saves all files"},
		
	CommandDefinition {
		.name = "goto-prev-diagnostic-record",
		.description = "goes to the previous diagnostic record"},
	CommandDefinition {
		.name = "goto-next-diagnostic-record",
		.description = "goes to the next diagnostic record"},
	CommandDefinition {
		.name = "open-in-windows-explorer",
		.description = "opens the current file location in Windows Explorer"},
	
	CommandDefinition {
		.name = "editor-open-search",
		.description = "opens the editor search",
		.parameters = std::span {&editorOpenSearchBarParameter, 1u}},
	CommandDefinition {
		.name = "editor-open-goto-line",
		.description = "opens the editor goto line"},
	CommandDefinition {
		.name = "editor-open-diagnostics-list",
		.description = "opens the diagnostics list"},
	CommandDefinition {
		.name = "editor-show-signature-help",
		.description = "shows the signature help if the current language supports it"},
	CommandDefinition {
		.name = "editor-show-autocomplete",
		.description = "shows the autocomplete suggestions if the current language supports it"},
	CommandDefinition {
		.name = "editor-show-goto-location",
		.description = "shows the goto location menu"},
	CommandDefinition {
		.name = "editor-save-file",
		.description = "saves the current file"},
	CommandDefinition {
		.name = "editor-scroll-up",
		.description = "scrolls the editor up",
		.parameters =  std::span {&editorScrollParameter, 1u}},
	CommandDefinition {
		.name = "editor-scroll-down",
		.description = "scrolls the editor down",
		.parameters =  std::span {&editorScrollParameter, 1u}},
	
	CommandDefinition {
		.name = "move-caret",
		.description = "move the caret to the specified position",
		.parameters =  moveCaretParameter,
		.forceParameterConfiguration = true},
	CommandDefinition {
		.name = "insert-text",
		.description = "inserts text at the current caret position",
		.parameters =  std::span {&insertTextParameter, 1u},
		.forceParameterConfiguration = true},
	CommandDefinition {
		.name = "delete-range",
		.description = "deletes the specified range of text",
		.parameters =  std::span {deleteRangeParameter, 1u},
		.forceParameterConfiguration = true},
	
	CommandDefinition {
		.name = "select-all",
		.description = "selects all text in the current buffer"},
	CommandDefinition {
		.name = "select-line",
		.description = "selects the current line"},
	CommandDefinition {
		.name = "select-in-brackets",
		.description = "selects the text inside the nearest brackets"},
	CommandDefinition {
		.name = "select-word",
		.description = "selects the word at the current caret position"},
	
	CommandDefinition {
		.name = "repeat-text",
		.description = "inserts the specified text multiple times at the current caret position",
		.parameters =  std::span {repeatTextParameters},
		.forceParameterConfiguration = true},
	CommandDefinition {
		.name = "transform-case",
		.description = "changes the case of the selected text",
		.parameters =  transformCaseParameter,
		.forceParameterConfiguration = true},
	CommandDefinition {
		.name = "to-upper-case",
		.description = "transforms the selected text to UPPER CASE"},
	CommandDefinition {
		.name = "to-lower-case",
		.description = "transforms the selected text to lower case"},
	
	CommandDefinition {
		.name = "delete-line",
		.description = "deletes the current line"},
	CommandDefinition {
		.name = "indent-line",
		.description = "indents the current line"},
	CommandDefinition {
		.name = "unindent-line",
		.description = "unindents the current line"},
	CommandDefinition {
		.name = "comment-line",
		.description = "comments or uncomments all selected line if the current language supports it",
		.parameters = std::span {&commentLineParameter, 1u}},
	CommandDefinition {
		.name = "duplicate-line",
		.description = "duplicates the current line"},
	CommandDefinition {
		.name = "swap-lines",
		.description = "swaps the current line with the next line"},
	CommandDefinition {
		.name = "trim-trailing-whitespace",
		.description = "trims trailing whitespace from the current line"},
		
	CommandDefinition {
		.name = "undo",
		.description = "undos the last text change"},
	CommandDefinition {
		.name = "redo",
		.description = "redoes the last undone text change"},
	
	CommandDefinition {
		.name = "cut",
		.description = "cuts the selected text to the clipboard"},
	CommandDefinition {
		.name = "copy",
		.description = "copies the selected text to the clipboard"},
	CommandDefinition {
		.name = "paste",
		.description = "pastes the text from the clipboard"},
	CommandDefinition {
		.name = "cutLines",
		.description = "cuts the selected lines to the clipboard"},
		
	CommandDefinition {
		.name = "toggle-edit-mode",
		.description = "toggles the multi-caret edit mode"},
	CommandDefinition {
		.name = "add-caret-above",
		.description = "adds a new caret above the current caret"},
	CommandDefinition {
		.name = "add-caret-below",
		.description = "adds a new caret below the current caret"},
	CommandDefinition {
		.name = "goto-next-caret",
		.description = "moves the edit-caret to the next caret position"},
	CommandDefinition {
		.name = "goto-prev-caret",
		.description = "moves the edit-caret to the previous caret position"},
	CommandDefinition {
		.name = "align-carets-right",
		.description = "aligns all carets to the right"},
	CommandDefinition {
		.name = "align-carets-left",
		.description = "aligns all carets to the left"},
	CommandDefinition {
		.name = "insert-numbers",
		.description = "insert incrementing numbers at each caret position",
		.parameters =  std::span {&insertNumbersParameter, 1u},
		.forceParameterConfiguration = true},
	
	CommandDefinition {
		.name = "shell-execute",
		.description = "executes the selected file"},
	CommandDefinition {
		.name = "new-directory",
		.description = "creates a new directory"},
	CommandDefinition {
		.name = "rename",
		.description = "renames the selected file or directory"},
		
	CommandDefinition {
		.name = "tool-output-terminate-process",
		.description = "terminates the tool output process"},
};

static_assert(STATIC_ARRAY_SIZE(commandDefinitions) == Command::COUNT);
