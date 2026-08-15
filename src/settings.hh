#pragma once
#include "events.hh"
#include "commands.hh"
#include "util/color.hh"
#include "graphics/font.hh"

#include <unordered_map>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct ID2D1Bitmap;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
namespace std {
	template<> struct hash<KeyEvent> {
		std::size_t operator()(KeyEvent event) const {
			std::hash<u32> u32Hash;
			return u32Hash(event.vkeycode) ^ u32Hash(event.modifiers);
		}
	};
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct Settings {

	//-----------------------------------------------------
	// icons
	//-----------------------------------------------------
		
	struct Icons {
		 ID2D1Bitmap* unknown = nullptr;
		 ID2D1Bitmap* waiting = nullptr;
		 ID2D1Bitmap* error = nullptr;
		 ID2D1Bitmap* noItems = nullptr;
		 
		 ID2D1Bitmap* tabsModifiedHovered = nullptr;
		 ID2D1Bitmap* tabsModified = nullptr;
		 ID2D1Bitmap* tabsHovered = nullptr;
	
		 ID2D1Bitmap* editorDiagnosticsError = nullptr;
		 ID2D1Bitmap* editorDiagnosticsWarning = nullptr;
		 ID2D1Bitmap* editorDiagnosticsInfo = nullptr;
		 ID2D1Bitmap* editorDiagnosticsHint = nullptr;

		 ID2D1Bitmap* editorAutocompleteText = nullptr;
		 ID2D1Bitmap* editorAutocompleteMethod = nullptr;
		 ID2D1Bitmap* editorAutocompleteFunction = nullptr;
		 ID2D1Bitmap* editorAutocompleteConstructor = nullptr;
		 ID2D1Bitmap* editorAutocompleteField = nullptr;
		 ID2D1Bitmap* editorAutocompleteVariable = nullptr;
		 ID2D1Bitmap* editorAutocompleteClass = nullptr;
		 ID2D1Bitmap* editorAutocompleteInterface = nullptr;
		 ID2D1Bitmap* editorAutocompleteModule = nullptr;
		 ID2D1Bitmap* editorAutocompleteProperty = nullptr;
		 ID2D1Bitmap* editorAutocompleteUnit = nullptr;
		 ID2D1Bitmap* editorAutocompleteValue = nullptr;
		 ID2D1Bitmap* editorAutocompleteEnum = nullptr;
		 ID2D1Bitmap* editorAutocompleteKeyword = nullptr;
		 ID2D1Bitmap* editorAutocompleteSnippet = nullptr;
		 ID2D1Bitmap* editorAutocompleteColor = nullptr;
		 ID2D1Bitmap* editorAutocompleteFile = nullptr;
		 ID2D1Bitmap* editorAutocompleteReference = nullptr;
		 ID2D1Bitmap* editorAutocompleteFolder = nullptr;
		 ID2D1Bitmap* editorAutocompleteEnumMember = nullptr;
		 ID2D1Bitmap* editorAutocompleteConstant = nullptr;
		 ID2D1Bitmap* editorAutocompleteStruct = nullptr;
		 ID2D1Bitmap* editorAutocompleteEvent = nullptr;
		 ID2D1Bitmap* editorAutocompleteOperator = nullptr;
		 ID2D1Bitmap* editorAutocompleteTypeParameter = nullptr;

		 ID2D1Bitmap* editorSearchResultsClosed = nullptr;
		 ID2D1Bitmap* editorSearchResultsOpened = nullptr;
		 
		 ID2D1Bitmap* explorerFolderOpen = nullptr;
		 ID2D1Bitmap* explorerFolderClosed = nullptr;
		 ID2D1Bitmap* explorerFile = nullptr;
		 
		 ID2D1Bitmap* lspStandby = nullptr;
		 ID2D1Bitmap* lspInitializing = nullptr;
		 ID2D1Bitmap* lspRunning = nullptr;
		 ID2D1Bitmap* lspShuttingDown = nullptr;
		 ID2D1Bitmap* lspExited = nullptr;
		 ID2D1Bitmap* lspCrashed = nullptr;
	};
	
	//-----------------------------------------------------
	// colors
	//-----------------------------------------------------
	
	struct Colors {
		Color unknown              = {1.0f, 0.0f, 0.1f, 1.0f};
		Color dropShadow           = {0.2f, 0.3f, 0.6f, 1.0f}; // @TODO was "glow" - don't forget to change settings.json!!
		Color activePanelFrame     = {0.2f, 0.3f, 0.6f, 1.0f};
		Color selection            = {0.0f, 1.0f, 1.0f, 0.3f};
		Color selectionInactive    = {1.0f, 1.0f, 1.0f, 0.3f};
		Color hover                = {1.0f, 1.0f, 1.0f, 0.5f};
		Color pressed              = {0.8f, 0.8f, 0.8f, 0.5f};
		Color toggled              = {0.8f, 0.8f, 0.8f, 0.5f};
		Color editorText           = {1.0f, 1.0f, 1.0f, 1.0f};
		Color editorBackground     = {0.1f, 0.1f, 0.1f, 1.0f};
		Color editorMultiCaretEdit = {1.0f, 0.0f, 1.0f, 1.0f};
		Color uiText               = {1.0f, 1.0f, 1.0f, 1.0f};
		Color uiTextInactive       = {0.6f, 0.6f, 0.6f, 1.0f};
		Color uiSearchResult       = {1.0f, 1.0f, 0.0f, 0.3f};
		Color uiBackground         = {0.3f, 0.3f, 0.3f, 1.0f};
		Color uiBackgroundInactive = {0.2f, 0.2f, 0.2f, 1.0f};
		Color uiBackgroundInvalid  = {0.4f, 0.0f, 0.0f, 1.0f};
	};
	
	//-----------------------------------------------------
	// keybinds
	//-----------------------------------------------------
	
	struct KeyBind {
		Command::Id commandId = Command::Id_None;
		std::vector<ParameterValue> parameters = {};
	};
	
	//-----------------------------------------------------
	// statics
	//-----------------------------------------------------
	
	static constexpr u64 NUM_ICONS = sizeof(Icons) / sizeof(ID2D1Bitmap*);
	static constexpr u64 NUM_COLORS  = sizeof(Colors) / sizeof(Color);

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	union {
		Icons icons = {};
		ID2D1Bitmap* iconArray[NUM_ICONS];
	};

	union {
		Colors colors = {};
		Color colorArray[NUM_COLORS];
	};
	
	std::unordered_map<KeyEvent, KeyBind> keyBinds = {};
		
	Font fontUi = {};
	Font fontEditor = {};
	
	f32 scrollbarMarkerHoverDistance = 10.0f;
	bool backupFileBeforeSaving = false; // @TODO not implemented
	
	u64 jsonAllocatorNodeCapacity = 64u;
	u64 jsonAllocatorStringCapacity = 512u;
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	bool Init(ID2D1DeviceContext* deviceContext);
	~Settings() noexcept;
	
	Command LookupKeyBind(KeyEvent ev);
	
	ID2D1SolidColorBrush* GetBrushDropShadow();
	ID2D1SolidColorBrush* GetBrushSelection(bool active = true);
	ID2D1SolidColorBrush* GetBrushHover(bool pressed = false);
	ID2D1SolidColorBrush* GetBrushToggled();
	ID2D1SolidColorBrush* GetBrushEditorText();
	ID2D1SolidColorBrush* GetBrushEditorBackground();
	ID2D1SolidColorBrush* GetBrushEditorMultiCaretEdit();
	ID2D1SolidColorBrush* GetBrushUiSearchResult();
	ID2D1SolidColorBrush* GetBrushUiText(bool active = true);
	ID2D1SolidColorBrush* GetBrushUiBackground(bool active = true);
	ID2D1SolidColorBrush* GetBrushUiBackgroundInvalid();
};

extern Settings settings;
