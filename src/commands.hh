#pragma once
#include "util/parameter.hh"

#include <string_view>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct Command {
	enum Id {
		Id_None = 0,
		Id_OpenFileSearch,
		Id_OpenToolSearch,
		Id_OpenCommandSearch,
		Id_ToggleToolOutput,
		Id_ToggleExplorer,
		Id_FocusNextTab,
		Id_FocusPrevTab,
		Id_FocusNextPanel,
		Id_FocusPrevPanel,
		Id_SwapPanels,
		Id_ClosePanel,
		Id_AddPanelAfter,
		Id_AddPanelBefore,
		Id_NewFile,
		Id_CloseFile,
		Id_CloseFilesToTheRight,
		Id_CloseFilesToTheLeft,
		Id_CloseOtherFiles,
		Id_ClosePanelAndFile,
		Id_SaveAll,
		
		Id_GotoPrevDiagnosticRecord,
		Id_GotoNextDiagnosticRecord,
		Id_OpenInWindowsExplorer,
		
		Id_Editor_OpenSearch,
		Id_Editor_OpenGotoLine,
		Id_Editor_OpenDiagnosticsList,
		Id_Editor_ShowSignatureHelp,
		Id_Editor_ShowAutocomplete,
		Id_Editor_ShowGotoLocation,
		Id_Editor_SaveFile,
		Id_Editor_ScrollUp,
		Id_Editor_ScrollDown,
		
		Id_Text_MoveCaret,		
		Id_Text_InsertText,
		Id_Text_DeleteRange,
		
		Id_Text_SelectAll,
		Id_Text_SelectLine,
		Id_Text_SelectInBrackets,
		Id_Text_SelectWord,
		
		Id_Text_RepeatText,
		Id_Text_TransformCase,
		Id_Text_ToUpperCase,
		Id_Text_ToLowerCase,
		
		Id_Text_DeleteLine,
		Id_Text_IndentLine,
		Id_Text_UnIndentLine,
		Id_Text_CommentLine,
		Id_Text_DuplicateLine,
		Id_Text_SwapLines,
		Id_Text_TrimTrailingWhitespace,
		
		Id_Text_Undo,
		Id_Text_Redo,
		
		Id_Clipboard_Cut,
		Id_Clipboard_Copy,
		Id_Clipboard_Paste,
		Id_Clipboard_CutLines,
		
		Id_MultiCaret_ToggleEditMode,
		Id_MultiCaret_AddCaretAbove,
		Id_MultiCaret_AddCaretBelow,
		Id_MultiCaret_GotoNextCaret,
		Id_MultiCaret_GotoPrevCaret,
		Id_MultiCaret_AlignCaretsRight,
		Id_MultiCaret_AlignCaretsLeft,
		Id_MultiCaret_InsertNumbers,
		
		Id_Explorer_ShellExecute,
		Id_Explorer_NewDirectory,
		Id_Explorer_Rename,
		
		Id_ToolOutput_TerminateProcess,
		
		COUNT
	};

	Id id = Id_None;
	std::span<ParameterValue> parameters = {};
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct CommandDefinition {
	std::string_view name = {};
	std::string_view description = {};
	
	std::span<ParameterDefinition> parameters = {};
	
	bool forceParameterConfiguration = false;
};

// @FIXME why is this not const?
extern CommandDefinition commandDefinitions[];
