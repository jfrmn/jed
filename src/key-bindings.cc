#include "key-bindings.hh"
#include "json/json-mapping.hh"

#include <unordered_map>

#include <cJSON/cJSON.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// These values a ripped from windows.h including some comments.
// Some virtual keycodes have been left out like the mouse buttons
// VK_LBUTTON, VK_RBUTTON.
// Some others that ARE in here might not really make sense in the real world
// but im it is what it is. 

static constexpr std::pair<std::string_view, u32> stringToVirtualKeyCode[] = {
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
	
	"consoleTerminateProcess",
	"consoleCopy"
};

static_assert(STATIC_ARRAY_SIZE(actionNames) == KeyBindings::NUM_ACTIONS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
KeyBindings keybinds {
	.actions = {
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

static_assert(STATIC_ARRAY_SIZE(keybinds.array) == KeyBindings::NUM_ACTIONS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool KeyEventFromString(const JsonTrace* trace, std::string_view str, /*out*/ KeyEvent* result) {
			
	std::string_view remaining = str;
	while (!remaining.empty()) {
		const usize posDelim =  remaining.find('-');
		
		const std::string_view token = remaining.substr(0, posDelim);
		remaining = posDelim != std::string_view::npos ? remaining.substr(posDelim + 1) : std::string_view {};
		
		if (token == "C" || token == "Ctrl" || token == "ctrl") {
			result->ctrl = true;
		
		} else if (token == "A" || token == "Alt" || token == "alt") {
			result->alt = true;
		
		} else if (token == "S" || token == "Shift" || token == "shift") {
			result->shift = true;
		
		} else if (!token.empty() && token[0] >= 'a' && token[0] <= 'z') {
			result->vkeycode = toupper(token[0]);
		
		} else {
			for (const auto& kvp : stringToVirtualKeyCode) {
				if (kvp.first == token) {
					result->vkeycode = kvp.second;
					goto found;
				}
			}
			
			JsonLogError(trace, "Unrecognized virtual keycode '%'", token);
			found: continue;
		}
	}
		
	return result;
};

static u64 ActionFromString(std::string_view str) {
	if (str.empty()) return false;

	for (u64 i = 0; i < KeyBindings::NUM_ACTIONS; i++) {
		if (str == actionNames[i]) {
			if (str == actionNames[i])
				return i;
		}
	}
	return U64_MAX;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool KeyBindings::Init(const cJSON* json) {
	
	// insert user keybinds
	if (const cJSON* jsonKeybinds = cJSON_GetObjectItem(json, "key-bindings")) {
		const JsonTrace traceKeybinds {nullptr, "key-bindings"};
		
		std::unordered_map<std::string_view, cJSON*> userKeybinds {};
		if (!JsonObjectToMap(&traceKeybinds, jsonKeybinds, &userKeybinds))
			return true;
		
		for (auto it = userKeybinds.begin(); it != userKeybinds.end(); ++it) {
			const JsonTrace trace {&traceKeybinds, it->first};
			
			const u64 actionIndex = ActionFromString(it->first);
			if (actionIndex == U64_MAX) {
				JsonLogWarning(&trace, "unknown action '%'", it->first);
				continue;
			}
			
			if (cJSON_IsNull(it->second)) {
				array[actionIndex] = KeyEvent {
					.vkeycode = VK_NONE};
									
			} else if (cJSON_IsString(it->second)) {
				KeyEventFromString(&trace, it->second->valuestring, &array[actionIndex]);
			
			} else {
				JsonLogWarning(&trace, "expected either a [string] or [null] but was [%]", JsonTypeToString(it->second->type));
			}
		}
	}
	
	return true;
}