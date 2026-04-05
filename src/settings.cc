#include "settings.hh"
#include "graphics/factories.hh"
#include "graphics/effects.hh"

#include "util/file-util.hh"
#include "util/string-util.hh"
#include "util/logging.hh"

#include "commands/tools.hh"

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 1
#include <toml++/toml.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>
#include <wincodec.h>
#include <dwrite_1.h> // @FIXME only needed to release font face - handle that in Font 
#include <Windows.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Boiler Plate
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static constexpr std::string_view colorNames[] {
	"unknown",
	"drop-shadow",
	"active-panel-frame",
	"selection",
	"selection-inactive",
	"ui-hover",
	"ui-pressed",
	"ui-toggled",
	"editor-text",
	"editor-background",
	"editor-multi-caret-edit",
	"ui-text",
	"ui-text-inactive",
	"ui-search-result",
	"ui-background",
	"ui-background-inactive",
	"ui-background-invalid"
};

static_assert(STATIC_ARRAY_SIZE(colorNames) == Settings::NUM_COLORS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::string_view iconNames[] {
	"unknown",
	"waiting",
	"error",
	"noitems",
	"tabs-modified-hovered",
	"tabs-modified",
	"tabs-hovered",
	"editor-diagnostics-error",
	"editor-diagnostics-warning",
	"editor-diagnostics-info",
	"editor-diagnostics-hint",
	"editor-autocomplete-text",
	"editor-autocomplete-method",
	"editor-autocomplete-function",
	"editor-autocomplete-constructor",
	"editor-autocomplete-field",
	"editor-autocomplete-variable",
	"editor-autocomplete-class",
	"editor-autocomplete-interface",
	"editor-autocomplete-module",
	"editor-autocomplete-property",
	"editor-autocomplete-unit",
	"editor-autocomplete-value",
	"editor-autocomplete-enum",
	"editor-autocomplete-keyword",
	"editor-autocomplete-snippet",
	"editor-autocomplete-color",
	"editor-autocomplete-file",
	"editor-autocomplete-reference",
	"editor-autocomplete-folder",
	"editor-autocomplete-enum-member",
	"editor-autocomplete-constant",
	"editor-autocomplete-struct",
	"editor-autocomplete-event",
	"editor-autocomplete-operator",
	"editor-autocomplete-type-parameter",
	"editor-search-resultsclosed",
	"editor-search-resultsopened",
	"explorer-folder-open",
	"explorer-folder-closed",
	"explorer-file",
	"lsp-standby",
	"lsp-initializing",
	"lsp-running",
	"lsp-shuttingDown",
	"lsp-exited",
	"lsp-crashed"
};

static_assert(STATIC_ARRAY_SIZE(iconNames) == Settings::NUM_ICONS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// These values a ripped from windows.h including some comments.
// Some virtual keycodes have been left out like the mouse buttons
// VK_LBUTTON, VK_RBUTTON.
// Some others that ARE in here might not really make sense in the real world

static constexpr std::pair<std::string_view, u32> stringToVirtualKeyCode[] = {
	{"NONE", VK_NONE}, 
	{"UNBOUND", VK_NONE}, 

	{"BACK", VK_BACK}, 
	{"TAB",  VK_TAB}, 
	
	{"CLEAR",  VK_CLEAR}, 
	{"RETURN", VK_RETURN}, 
	
	{"SHIFT",   VK_SHIFT}, 
	{"CONTROL", VK_CONTROL}, 
	{"MENU",    VK_MENU}, 
	{"PAUSE",   VK_PAUSE}, 
	{"CAPITAL", VK_CAPITAL}, 
	
	{"KANA",    VK_KANA},
	{"HANGEUL", VK_HANGEUL}, /* old name - should be here for compatibility */
	{"HANGUL",  VK_HANGUL}, 
	{"IME_ON",  VK_IME_ON}, 
	{"JUNJA",   VK_JUNJA}, 
	{"FINAL",   VK_FINAL}, 
	{"HANJA",   VK_HANJA}, 
	{"KANJI",   VK_KANJI}, 
	{"IME_OFF", VK_IME_OFF}, 
	
	{"ESCAPE", VK_ESCAPE}, 
	
	{"CONVERT",    VK_CONVERT}, 
	{"NONCONVERT", VK_NONCONVERT}, 
	{"ACCEPT",     VK_ACCEPT}, 
	{"MODECHANGE", VK_MODECHANGE}, 
	
	{"SPACE",    VK_SPACE}, 
	{"PRIOR",    VK_PRIOR}, 
	{"NEXT",     VK_NEXT}, 
	{"END",      VK_END}, 
	{"HOME",     VK_HOME}, 
	{"LEFT",     VK_LEFT}, 
	{"UP",       VK_UP}, 
	{"RIGHT",    VK_RIGHT}, 
	{"DOWN",     VK_DOWN}, 
	{"SELECT",   VK_SELECT}, 
	{"PRINT",    VK_PRINT}, 
	{"EXECUTE",  VK_EXECUTE}, 
	{"SNAPSHOT", VK_SNAPSHOT}, 
	{"INSERT",   VK_INSERT}, 
	{"DELETE",   VK_DELETE}, 
	{"HELP",     VK_HELP}, 
	
	{"LWIN", VK_LWIN}, 
	{"RWIN", VK_RWIN}, 
	{"APPS", VK_APPS}, 
	
	{"SLEEP", VK_SLEEP}, 
	
	{"NUMPAD0",   VK_NUMPAD0},
	{"NUMPAD1",   VK_NUMPAD1}, 
	{"NUMPAD2",   VK_NUMPAD2}, 
	{"NUMPAD3",   VK_NUMPAD3}, 
	{"NUMPAD4",   VK_NUMPAD4}, 
	{"NUMPAD5",   VK_NUMPAD5}, 
	{"NUMPAD6",   VK_NUMPAD6}, 
	{"NUMPAD7",   VK_NUMPAD7}, 
	{"NUMPAD8",   VK_NUMPAD8}, 
	{"NUMPAD9",   VK_NUMPAD9}, 
	{"MULTIPLY",  VK_MULTIPLY}, 
	{"ADD",       VK_ADD}, 
	{"SEPARATOR", VK_SEPARATOR}, 
	{"SUBTRACT",  VK_SUBTRACT}, 
	{"DECIMAL",   VK_DECIMAL}, 
	{"DIVIDE",    VK_DIVIDE}, 
	
	{"F1",  VK_F1}, 
	{"F2",  VK_F2}, 
	{"F3",  VK_F3}, 
	{"F4",  VK_F4}, 
	{"F5",  VK_F5}, 
	{"F6",  VK_F6}, 
	{"F7",  VK_F7}, 
	{"F8",  VK_F8}, 
	{"F9",  VK_F9}, 
	{"F10", VK_F10}, 
	{"F11", VK_F11}, 
	{"F12", VK_F12}, 
	{"F13", VK_F13}, 
	{"F14", VK_F14}, 
	{"F15", VK_F15}, 
	{"F16", VK_F16}, 
	{"F17", VK_F17}, 
	{"F18", VK_F18}, 
	{"F19", VK_F19}, 
	{"F20", VK_F20}, 
	{"F21", VK_F21}, 
	{"F22", VK_F22}, 
	{"F23", VK_F23}, 
	{"F24", VK_F24}, 
 	
 	/* 0x88 - 0x8F : UI navigation */
	{"NAVIGATION_VIEW",   VK_NAVIGATION_VIEW},  // reserved
	{"NAVIGATION_MENU",   VK_NAVIGATION_MENU},  // reserved
	{"NAVIGATION_UP",     VK_NAVIGATION_UP},  // reserved
	{"NAVIGATION_DOWN",   VK_NAVIGATION_DOWN},  // reserved
	{"NAVIGATION_LEFT",   VK_NAVIGATION_LEFT},  // reserved
	{"NAVIGATION_RIGHT",  VK_NAVIGATION_RIGHT},  // reserved
	{"NAVIGATION_ACCEPT", VK_NAVIGATION_ACCEPT},  // reserved
	{"NAVIGATION_CANCEL", VK_NAVIGATION_CANCEL},  // reserved
	
	{"NUMLOCK", VK_NUMLOCK}, 
	{"SCROLL",  VK_SCROLL}, 
 	
 	/* NEC PC-9800 kbd definitions */
	{"OEM_NEC_EQUAL", VK_OEM_NEC_EQUAL},    // '=' key on numpad
	
 	/* Fujitsu/OASYS kbd definitions */
	{"OEM_FJ_JISHO",        VK_OEM_FJ_JISHO},    // 'Dictionary' key
	{"OEM_FJ_MASSHOU",      VK_OEM_FJ_MASSHOU},    // 'Unregister word' key
	{"OEM_FJ_TOUROKU",      VK_OEM_FJ_TOUROKU},    // 'Register word' key
	{"OEM_FJ_LOYA",         VK_OEM_FJ_LOYA},    // 'Left OYAYUBI' key
	{"OEM_FJ_ROYA",         VK_OEM_FJ_ROYA},    // 'Right OYAYUBI' key
	{"BROWSER_BACK",        VK_BROWSER_BACK}, 
	{"BROWSER_FORWARD",     VK_BROWSER_FORWARD}, 
	{"BROWSER_REFRESH",     VK_BROWSER_REFRESH}, 
	{"BROWSER_STOP",        VK_BROWSER_STOP}, 
	{"BROWSER_SEARCH",      VK_BROWSER_SEARCH}, 
	{"BROWSER_FAVORITES",   VK_BROWSER_FAVORITES}, 
	{"BROWSER_HOME",        VK_BROWSER_HOME}, 
	{"VOLUME_MUTE",         VK_VOLUME_MUTE}, 
	{"VOLUME_DOWN",         VK_VOLUME_DOWN}, 
	{"VOLUME_UP",           VK_VOLUME_UP}, 
	{"MEDIA_NEXT_TRACK",    VK_MEDIA_NEXT_TRACK}, 
	{"MEDIA_PREV_TRACK",    VK_MEDIA_PREV_TRACK}, 
	{"MEDIA_STOP",          VK_MEDIA_STOP}, 
	{"MEDIA_PLAY_PAUSE",    VK_MEDIA_PLAY_PAUSE}, 
	{"LAUNCH_MAIL",         VK_LAUNCH_MAIL}, 
	{"LAUNCH_MEDIA_SELECT", VK_LAUNCH_MEDIA_SELECT}, 
	{"LAUNCH_APP1",         VK_LAUNCH_APP1}, 
	{"LAUNCH_APP2",         VK_LAUNCH_APP2}, 
	{"OEM_1",               VK_OEM_1},    // ';:' for US
	{"OEM_PLUS",            VK_OEM_PLUS},    // '+' any country
	{"OEM_COMMA",           VK_OEM_COMMA},    // ',' any country
	{"OEM_MINUS",           VK_OEM_MINUS},    // '-' any country
	{"OEM_PERIOD",          VK_OEM_PERIOD},    // '.' any country
	{"OEM_2",               VK_OEM_2},    // '/?' for US
	{"OEM_3",               VK_OEM_3},    // '`~' for US
	{"OEM_4",               VK_OEM_4},   //  '[{' for US
	{"OEM_5",               VK_OEM_5},   //  '\|' for US
	{"OEM_6",               VK_OEM_6},   //  ']}' for US
	{"OEM_7",               VK_OEM_7},   //  ''"' for US
	{"OEM_8",               VK_OEM_8}, 
	{"OEM_AX",              VK_OEM_AX},   //  'AX' key on Japanese AX kbd
	{"OEM_102",             VK_OEM_102},   //  "<>" or "\|" on RT 102-key kbd.
	{"ICO_HELP",            VK_ICO_HELP},   //  Help key on ICO
	{"ICO_00",              VK_ICO_00},   //  00 key on ICO
	{"PROCESSKEY",          VK_PROCESSKEY}, 
	{"ICO_CLEAR",           VK_ICO_CLEAR}, 
	{"PACKET",              VK_PACKET}, 
 	
 	/* Nokia/Ericsson definition */
	{"OEM_RESET",   VK_OEM_RESET}, 
	{"OEM_JUMP",    VK_OEM_JUMP}, 
	{"OEM_PA1",     VK_OEM_PA1}, 
	{"OEM_PA2",     VK_OEM_PA2}, 
	{"OEM_PA3",     VK_OEM_PA3}, 
	{"OEM_WSCTRL",  VK_OEM_WSCTRL}, 
	{"OEM_CUSEL",   VK_OEM_CUSEL}, 
	{"OEM_ATTN",    VK_OEM_ATTN}, 
	{"OEM_FINISH",  VK_OEM_FINISH}, 
	{"OEM_COPY",    VK_OEM_COPY}, 
	{"OEM_AUTO",    VK_OEM_AUTO}, 
	{"OEM_ENLW",    VK_OEM_ENLW}, 
	{"OEM_BACKTAB", VK_OEM_BACKTAB}, 
	
	{"ATTN",      VK_ATTN}, 
	{"CRSEL",     VK_CRSEL}, 
	{"EXSEL",     VK_EXSEL}, 
	{"EREOF",     VK_EREOF}, 
	{"PLAY",      VK_PLAY}, 
	{"ZOOM",      VK_ZOOM}, 
	{"NONAME",    VK_NONAME}, 
	{"PA1",       VK_PA1}, 
	{"OEM_CLEAR", VK_OEM_CLEAR}
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::string_view actionNames[] = {
	"show-file-search",
	"show-explorer",
	"show-tool-search",
	"show-console",
	
	"focus-next-tab",
	"focus-prev-tab",
	"focus-next-panel",
	"focus-prev-panel",
	"add-panel-after",
	"add-panel-before",
	"swap-panels",
	"close-panel",
	"close-panel-and-tab",
	"close-tab",
	
	"open-search",
	"open-search-and-replace",
	"open-goto-line",
	"show-signature-help",
	"show-autocomplete",
	"show-goto-location",
	"save-file",
	"scroll-up",
	"scroll-down",
	"goto-prev-diagnostic",
	"goto-next-diagnostic",
	
	"move-to-next-word",
	"move-to-prev-word",
	"move-to-line-start",
	"move-to-line-end",
	"move-to-buffer-start",
	"move-to-buffer-end",
	"move-page-up",
	"move-page-down",
	"select-backward",
	"select-forward",
	"select-line-up",
	"select-line-down",
	"select-to-next-word",
	"select-to-prev-word",
	"select-to-line-start",
	"select-to-line-end",
	"select-to-buffer-start",
	"select-to-buffer-end",
	"select-page-up",
	"select-page-down",
	"select-all",
	"select-line",
	"select-in-brackets",
	"select-word",
	"delete-prev-char",
	"delete-prev-word",
	"delete-next-char",
	"delete-next-word",
	"delete-line",
	"indent-line",
	"unindent-line",
	"insert-tab",
	"duplicate-line",
	"undo",
	"redo",
	"cut",
	"copy",
	"paste",
	"cut-lines",
	"line-comment",
	"line-uncomment",
	"block-comment",
	"block-uncomment",
	"add-caret-above",
	"add-caret-below",	
	"edit-carets",
		
	"explorer-shell-execute",
	"explorer-open-in-windows-explorer",
	"explorer-new-file",
	"explorer-new-folder",
	"explorer-rename",
	
	"console-terminate-process",
	"console-copy"
};

static_assert(STATIC_ARRAY_SIZE(actionNames) == Settings::NUM_KEYBINDS);

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Defaults
//
///////////////////////////////////////////////////////////////////////////////////////////////////

Settings settings {
	.icons = {},
	
	.colors = {
		.unknown              = {1.0f, 0.0f, 0.1f, 1.0f},
		.dropShadow           = {0.2f, 0.3f, 0.6f, 1.0f},
		.activePanelFrame     = {0.2f, 0.3f, 0.6f, 1.0f},
		.selection            = {0.0f, 1.0f, 1.0f, 0.3f},
		.selectionInactive    = {1.0f, 1.0f, 1.0f, 0.3f},
		.hover                = {1.0f, 1.0f, 1.0f, 0.5f},
		.pressed              = {0.8f, 0.8f, 0.8f, 0.5f},
		.toggled              = {0.8f, 0.8f, 0.8f, 0.5f},
		.editorText           = {1.0f, 1.0f, 1.0f, 1.0f},
		.editorBackground     = {0.1f, 0.1f, 0.1f, 1.0f},
		.editorMultiCaretEdit = {1.0f, 0.0f, 1.0f, 1.0f},
		.uiText               = {1.0f, 1.0f, 1.0f, 1.0f},
		.uiTextInactive       = {0.6f, 0.6f, 0.6f, 1.0f},
		.uiSearchResult       = {1.0f, 1.0f, 0.0f, 0.3f},
		.uiBackground         = {0.3f, 0.3f, 0.3f, 1.0f},
		.uiBackgroundInactive = {0.2f, 0.2f, 0.2f, 1.0f},
		.uiBackgroundInvalid  = {0.4f, 0.0f, 0.0f, 1.0f}},
	
	.keybinds  = {
		// window
		.showFileSearch       = {.vkeycode = VK_OEM_COMMA, .ctrl = true},
		.showExplorer         = {.vkeycode = 'E',          .ctrl = true},
		.showToolSearch       = {.vkeycode = 'T',          .ctrl = true},
		.showConsole          = {.vkeycode = VK_OEM_2,     .ctrl = true},
		.focusNextTab         = {.vkeycode = VK_TAB,       .ctrl = true},
		.focusPrevTab         = {.vkeycode = VK_TAB,       .ctrl = true, .shift = true},
		.focusNextPanel       = {.vkeycode = VK_RIGHT,                   .alt = true},
		.focusPrevPanel       = {.vkeycode = VK_LEFT,                    .alt = true},
		.addPanelAfter        = {.vkeycode = VK_LEFT,      .ctrl = true, .alt = true},
		.addPanelBefore       = {.vkeycode = VK_RIGHT,     .ctrl = true, .alt = true},
		.swapPanels           = {.vkeycode = VK_F10},
		.closePanel           = {.vkeycode = VK_F11},
		.closeTab             = {.vkeycode = VK_F12},
		.closePanelAndTab     = {.vkeycode = VK_F12,        .ctrl = true},

		// editor
		.openSearch           = {.vkeycode = 'F',          .ctrl = true},
		.openSearchAndReplace = {.vkeycode = 'H',          .ctrl = true},
		.openGotoLine         = {.vkeycode = 'G',          .ctrl = true},
		.showSignatureHelp    = {.vkeycode = 'I',          .ctrl = true},
		.showAutocomplete     = {.vkeycode = VK_SPACE,     .ctrl = true},
		.showGotoLocation     = {.vkeycode = VK_F8},
		.saveFile             = {.vkeycode = 'S',          .ctrl = true},
		.scrollUp             = {.vkeycode = VK_UP,        .ctrl = true},
		.scrollDown           = {.vkeycode = VK_DOWN,      .ctrl = true},
		.gotoPrevDiagnostic   = {.vkeycode = VK_PRIOR,     .ctrl = true},
		.gotoNextDiagnostic   = {.vkeycode = VK_NEXT,      .ctrl = true},

		// text controller
		.moveToPrevWord       = {.vkeycode = VK_LEFT,      .ctrl = true},
		.moveToNextWord       = {.vkeycode = VK_RIGHT,     .ctrl = true},
		.moveToLineStart      = {.vkeycode = VK_HOME},
		.moveToLineEnd        = {.vkeycode = VK_END},
		.moveToBufferStart    = {.vkeycode = VK_HOME,      .ctrl = true},
		.moveToBufferEnd      = {.vkeycode = VK_END,       .ctrl = true},
		.movePageUp           = {.vkeycode = VK_PRIOR},
		.movePageDown         = {.vkeycode = VK_NEXT},

		.selectBackward       = {.vkeycode = VK_LEFT,                    .shift = true},
		.selectForward        = {.vkeycode = VK_RIGHT,                   .shift = true},
		.selectLineUp         = {.vkeycode = VK_UP,                      .shift = true},
		.selectLineDown       = {.vkeycode = VK_DOWN,                    .shift = true},
		.selectToPrevWord     = {.vkeycode = VK_LEFT,      .ctrl = true, .shift = true},
		.selectToNextWord     = {.vkeycode = VK_RIGHT,     .ctrl = true, .shift = true},
		.selectToLineStart    = {.vkeycode = VK_HOME,                    .shift = true},
		.selectToLineEnd      = {.vkeycode = VK_END,                     .shift = true},
		.selectToBufferStart  = {.vkeycode = VK_HOME,      .ctrl = true, .shift = true},
		.selectToBufferEnd    = {.vkeycode = VK_END,       .ctrl = true, .shift = true},
		.selectPageUp         = {.vkeycode = VK_PRIOR,                   .shift = true},
		.selectPageDown       = {.vkeycode = VK_NEXT,                    .shift = true},
		.selectAll            = {.vkeycode = 'A',          .ctrl = true},
		.selectLine           = {.vkeycode = 'L',          .ctrl = true, .shift = true},
		.selectInBrackets     = {.vkeycode = 'B',          .ctrl = true, .shift = true},
		.selectWord           = {.vkeycode = 'W',          .ctrl = true},

		.deletePrevChar       = {.vkeycode = VK_BACK},
		.deleteNextChar       = {.vkeycode = VK_DELETE},
		.deletePrevWord       = {.vkeycode = VK_BACK,      .ctrl = true},
		.deleteNextWord       = {.vkeycode = VK_DELETE,    .ctrl = true},
		.deleteLine           = {.vkeycode = 'L',          .ctrl = true},

		.indentLine           = {.vkeycode = VK_TAB},
		.unindentLine         = {.vkeycode = VK_TAB,                    .shift = true},
		.insertTab            = {.vkeycode = VK_TAB,       .ctrl = true},
		.duplicateLine        = {.vkeycode = 'D',          .ctrl = true},

		.undo                 = {.vkeycode = 'Z',          .ctrl = true},
		.redo                 = {.vkeycode = 'Y',          .ctrl = true},
		.cut                  = {.vkeycode = 'X',          .ctrl = true},
		.copy                 = {.vkeycode = 'C',          .ctrl = true},
		.paste                = {.vkeycode = 'V',          .ctrl = true},
		.cutLines             = {.vkeycode = 'L',          .ctrl = true, .alt = true},
		.lineComment          = {.vkeycode = 'K',          .ctrl = true},
		.lineUncomment        = {.vkeycode = 'U',          .ctrl = true},
		.blockComment         = {.vkeycode = 'K',          .ctrl = true, .alt = true},
		.blockUncomment       = {.vkeycode = 'U',          .ctrl = true, .alt = true},
		
		.addCaretAbove        = {.vkeycode = VK_UP,        .ctrl = true, .alt = true},
		.addCaretBelow        = {.vkeycode = VK_DOWN,      .ctrl = true, .alt = true},
		.editCarets           = {.vkeycode = 'M',          .ctrl = true},
		
		.explorerShellExecute          = {.vkeycode = 'Q', .ctrl = true},
		.explorerOpenInWindowsExplorer = {.vkeycode = 'R', .ctrl = true, .shift = true},
		.explorerNewFile               = {.vkeycode = 'N', .ctrl = true},
		.explorerNewFolder             = {.vkeycode = 'N', .ctrl = true, .shift = true},
		.explorerRename                = {.vkeycode = VK_F2},
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Brush Function
//
///////////////////////////////////////////////////////////////////////////////////////////////////

ID2D1SolidColorBrush* Settings::GetBrushDropShadow() {
	brush->SetColor(colors.dropShadow.ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushSelection(bool active /*= true*/) {
	brush->SetColor((active
		? colors.selection
		: colors.selectionInactive).ToD2D());
	
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushUiSearchResult() {
	brush->SetColor(colors.uiSearchResult.ToD2D());	
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushEditorText() {
	brush->SetColor(colors.editorText.ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushEditorBackground() {
	brush->SetColor(colors.editorBackground.ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushEditorMultiCaretEdit() {
	brush->SetColor(colors.editorMultiCaretEdit.ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushUiText(bool active /*= true*/) {
	brush->SetColor((active
		? colors.uiText
		: colors.uiTextInactive).ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushUiBackground(bool active /*= true*/) {
	brush->SetColor((active
		? colors.uiBackground
		: colors.uiBackgroundInactive).ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushUiBackgroundInvalid() {
	brush->SetColor(colors.uiBackgroundInvalid.ToD2D());
	return brush;
}

ID2D1SolidColorBrush* Settings::GetBrushHover(bool pressed /*= false*/) {
	brush->SetColor((pressed
		? colors.pressed
		: colors.hover).ToD2D());
	return brush;
}
	
ID2D1SolidColorBrush* Settings::GetBrushToggled() {
	brush->SetColor(colors.toggled.ToD2D());
	return brush;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Loading
//
///////////////////////////////////////////////////////////////////////////////////////////////////

static ID2D1Bitmap* CreateDummyIcon(ID2D1DeviceContext* deviceContext) {
	
	ID2D1BitmapRenderTarget* bitmapRenderTarget = nullptr;
	if (HRESULT hr = deviceContext->CreateCompatibleRenderTarget({32.0f, 32.0f}, &bitmapRenderTarget); hr != S_OK) {
		LogError("CreateCompatibleRenderTarget() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	DEFER(bitmapRenderTarget->Release());
			
	bitmapRenderTarget->BeginDraw();
	bitmapRenderTarget->Clear(D2D1_COLOR_F {1.0f, 0.0f, 1.0f, 1.0f});
		
	if (HRESULT hr = bitmapRenderTarget->EndDraw(); hr != S_OK) {
		LogError("EndDraw() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
		
	ID2D1Bitmap* dummyIcon = nullptr;
	if (HRESULT hr = bitmapRenderTarget->GetBitmap(&dummyIcon); hr != S_OK) {
		LogError("GetBitmap() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	return dummyIcon;
}

static bool ToColor(const toml::node* node, /*out*/ Color* color) {
	
	if (node->is_array()) {
		const auto arr = node->as<toml::array>();
		ASSERT(arr);
		
		if (arr->size() < 3) LogWarning("%: insufficient number of values (expected 3 or 4)", node->source());
		if (arr->size() > 4) LogWarning("%: too many number of values (expected 3 or 4)", node->source());
		if (arr->empty()) return false;
				
		for (u64 i = 0u; i < std::min(4llu, arr->size()); i++) {
			auto nodeValue = arr->get(i);
			if (!nodeValue) {
				color->array[i] = 0.0f;
			} else if (nodeValue->is_integer()) {
				const s64 asInt = nodeValue->as_integer()->get();
				color->array[i] = asInt / 255.0f;
			} else if (nodeValue->is_floating_point()) {
				color->array[i] = static_cast<f32>(nodeValue->as_floating_point()->get());
			} else {
				LogWarning("%: invalid value type (should be int of float)", nodeValue->source());
				color->array[i] = 0.0f;
			}
		}
	
	} else if (node->is_table()) {
		const auto tbl = node->as<toml::table>();
		ASSERT(tbl);
	
		f32* channels[] {&color->r, &color->g, &color->b, &color->a};
		const char channelNames[] {'r', 'g', 'b', 'a'};
		
		for (u64 i = 0u; i < 4; i++) {
			const std::string_view channelName {&channelNames[i], 1u};
			const toml::node* nodeValue = tbl->get(channelName);
			
			if (!nodeValue) {
				if (channelNames[i] != 'a')
					LogWarning("%: missing entry '%'", tbl->source(), channelName);
				*channels[i] = 0.0f;
			} else if (nodeValue->is_integer()) {
				const s64 asInt = nodeValue->as_integer()->get();
				*channels[i] = asInt / 255.0f;
			} else if (nodeValue->is_floating_point()) {
				*channels[i] = static_cast<f32>(nodeValue->as_floating_point()->get());
			} else {
				LogWarning("%: invalid value type (should be int of float)", tbl->source());
				*channels[i] = 0.0f;
			}
		}
	
	} else if (node->is_string()) {
		// @TODO support more colors
		const std::string_view clrName = node->as_string()->get();
		if      (clrName == "red") *color = Color {1.0f, 0.0f, 0.0f, 1.0f};
		else if (clrName == "green") *color = Color {0.0f, 1.0f, 0.0f, 1.0f};
		else if (clrName == "blue") *color = Color {0.0f, 0.0f, 1.0f, 1.0f};
		else if (clrName == "yellow") *color = Color {1.0f, 1.0f, 0.0f, 1.0f};
		else if (clrName == "magenta") *color = Color {1.0f, 0.0f, 1.0f, 1.0f};
		else if (clrName == "cyan") *color = Color {0.0f, 1.0f, 1.0f, 1.0f};
		else if (clrName == "black") *color = Color {0.0f, 0.0f, 0.0f, 1.0f};
		else if (clrName == "white") *color = Color {1.0f, 1.0f, 1.0f, 1.0f};
		else if (clrName == "transparent") *color = Color {0.0f, 0.0f, 0.0f, 0.0f};
		else {
			LogError("%: unknown named color", node->source());
			return false;
		}
	
	} else {
		LogError("%: expected a table, array or string", node->source());
		return false;
	}
	
	return true;	
}

static bool LoadIcon(std::string_view utf8Path, ID2D1DeviceContext* deviceContext, /*out*/ ID2D1Bitmap** icon) {
	
	wchar filePath[MAX_PATH + 1];
	u64 length = 0;
	ToUtf16(utf8Path, filePath, &length);
	filePath[length] = '\0';

	if (GetFileAttributesW(filePath) == INVALID_FILE_ATTRIBUTES) {
		LogError("file does not exists: '%'", utf8Path);
		return false;
	}

	IWICBitmapDecoder* decoder = nullptr;
	if (HRESULT hr = wicFactory->CreateDecoderFromFilename(filePath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder); hr != S_OK) {
		LogError("CreateDecoderFromFilename() failed");
		return false;
	}
	DEFER(decoder->Release());

	IWICBitmapFrameDecode* frameDecode = nullptr;
	if (HRESULT hr = decoder->GetFrame(0, &frameDecode); hr != S_OK) {
		LogError("failed to decode frame. HRESULT: %", FHr(hr));
		return false;
	}
	DEFER(frameDecode->Release());
	
	IWICFormatConverter* converter = nullptr;
	if (HRESULT hr = wicFactory->CreateFormatConverter(&converter); hr != S_OK) {
		LogError("CreateFormatConverter() failed. HRESULT: %", FHr(hr));
		return false;
	}
	DEFER(converter->Release());
	
	if (HRESULT hr = converter->Initialize(
			frameDecode,
			GUID_WICPixelFormat32bppPRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut); hr != S_OK) {
		LogError("failed to initialize converter. HRESULT: %", FHr(hr));
		return false;
	}

	IWICBitmap* wicBitmap = nullptr;
	if (HRESULT hr = wicFactory->CreateBitmapFromSource(converter, WICBitmapCreateCacheOption::WICBitmapNoCache, &wicBitmap); hr != S_OK) {
		LogError("CreateBitmapFromSource() failed. HRESULT: %", FHr(hr));
		return false;
	}
	DEFER(wicBitmap->Release());
	
	ID2D1Bitmap* bitmap = nullptr;
	if (HRESULT hr = deviceContext->CreateBitmapFromWicBitmap(wicBitmap, &bitmap); hr != S_OK) {
		LogError("failed to create ID2D1Bitmap from IWICBitmap. HRESULT: %", FHr(hr));
		return false;
	}
	
	if (*icon)
	   (*icon)->Release();
	
	*icon = bitmap;
	return true;
}

static bool LoadIcon(const toml::node* node, ID2D1DeviceContext* deviceContext, /*out*/ ID2D1Bitmap** icon) {
	
	const toml::value<std::string>* value = node->as_string();
	if (!value) {
		LogError("%: expected a string", node->source());
		return false;
	}
	
	return LoadIcon(value->get(), deviceContext, icon);
}

static bool LoadFont(toml::node* node, /*out*/ Font* font) {
	Font::Description fontDescription {};
	
	toml::table* table = node->as_table();
	if (!table) {
		LogError("%: expected a table", node->source());
		return false;
	}
	
	//
	// name
	//
	if (toml::value<std::string>* valName = table->get_as<std::string>("name")) {
		fontDescription.name = std::move(valName->get());
	} else {
		LogError("%: expected entry 'name' as string", F(table->source()));
		return false;
	}
	
	//
	// size
	//
	{
		const toml::node* node = table->get("size");
		if (!node) {
			LogError("%: required entry 'size' not found", node->source());
			return false;
		}
		
		if (const auto* value = node->as_floating_point()) {
			fontDescription.size = static_cast<f32>(value->get());
		} else if (const auto* value = node->as_integer()) {
			fontDescription.size = static_cast<f32>(value->get());
		} else {
			LogError("%: expected a float or int", node->source());
			return false;
		}
	}
	
	//
	// weight, style and stretch
	//
	{
		if (const auto node = table->get_as<s64>("weight"))
			fontDescription.weight = node->get();
		
		if (const auto node = table->get_as<s64>("style"))
			fontDescription.style = node->get();
		
		if (const auto node = table->get_as<s64>("stretch"))
			fontDescription.stretch = node->get();
	}
	
	Font newFont {};
	if (!newFont.Init(fontDescription)) {
		LogError("failed to load font");
		return false;
	}
	
	if (font->fontFace)
		font->fontFace->Release();
	
	*font = newFont;
	newFont.fontFace = nullptr;
	return true;
}

bool Settings::Init(ID2D1DeviceContext* deviceContext) {
	
	//
	// load default icon
	//	
	ID2D1Bitmap* dummyIcon = CreateDummyIcon(deviceContext);
	DEFER(dummyIcon->Release());
	for (u64 i = 0u; i < NUM_ICONS; i++) {
		char buffer[MAX_PATH] {0};
		const u64 len = FormatToBuffer(buffer, "./assets/%.png", iconNames[i]);
		
		const bool ok = LoadIcon(std::string_view {buffer, len}, deviceContext, &iconArray[i]);
		
		// set to dummy icon if loading fail
		if (!ok) {
			iconArray[i] = dummyIcon;
			dummyIcon->AddRef();
		}
	}
	
	const std::string settingsFilepath = 
#ifdef _DEBUG
	 ".\\settings.toml";
#else
	GetProcessDirectory() + "\\settings.toml";
#endif

	
	//
	// parse file
	//
	LogInfo("loading settings from '%'", settingsFilepath);
	
	toml::parse_result result = toml::parse_file(settingsFilepath);
	
	if (result.failed()) {
		const toml::parse_error& error = result.error();
		LogError("file parse settings file '%'\nerror: % at %", settingsFilepath, error.description(), error.source());
		return false;
	}

	toml::table& table = result.table();
	
	//
	// load colors
	//
	if (const toml::table* tblColors = table.get_as<toml::table>("Colors")) {
		for (u64 i = 0; i < NUM_COLORS; i++) {
			const toml::node* nodeColor = tblColors->get(colorNames[i]);
			if (!nodeColor) continue;
			ToColor(nodeColor, &colorArray[i]);
		}
	}

	//
	// load icons
	//
	if (const toml::table* tblIcons = table.get_as<toml::table>("Icons")) {
		for (u64 i = 0u; i < NUM_ICONS; i++) {
			const toml::node* nodeIcon = tblIcons->get(iconNames[i]);
			if (!nodeIcon) continue;
			LoadIcon(nodeIcon, deviceContext, &iconArray[i]);
		}
	}
	
	//
	// load fonts
	//
	if (toml::table* tblFonts = table.get_as<toml::table>("Font")) {
		if (toml::node* nodeFontEditor = tblFonts->get("editor"))
			LoadFont(nodeFontEditor, &fontEditor);
		if (toml::node* nodeFontUi = tblFonts->get("ui"))
			LoadFont(nodeFontUi, &fontUi);
	}
	
	//
	// load tools
	//
	if (toml::node* nodeTools = table.get("Tools")) {
		Tool::LoadTools(nodeTools);
	}
	
	return true;
}
	
Settings::~Settings() noexcept {
	for (int i = 0; i < NUM_ICONS; i++) {
		if (iconArray[i])
			iconArray[i]->Release();
	}
}

