#pragma once
#include "graphics/font.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

struct cJSON;

struct Theme {
	
	//-----------------------------------------------------
	// types
	
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
	
	struct Colors {
		D2D_COLOR_F unknown = {};
		D2D_COLOR_F dropShadow = {}; // @TODO was "glow" - don't forget to change settings.json!!
		D2D_COLOR_F activePanelFrame = {};
		D2D_COLOR_F selection = {};
		D2D_COLOR_F selectionInactive = {};
		D2D_COLOR_F hover = {};
		D2D_COLOR_F pressed = {};
		D2D_COLOR_F toggled = {};
		D2D_COLOR_F editorText = {};
		D2D_COLOR_F editorBackground = {};
		D2D_COLOR_F editorMultiCaretEdit = {};
		D2D_COLOR_F uiText = {};
		D2D_COLOR_F uiTextInactive = {};
		D2D_COLOR_F uiSearchResult = {};
		D2D_COLOR_F uiBackground = {};
		D2D_COLOR_F uiBackgroundInactive = {};
		D2D_COLOR_F uiBackgroundInvalid = {};
	};
	
	//-----------------------------------------------------
	// data

	static constexpr u64 NUM_ICONS  = sizeof(Icons) / sizeof(ID2D1Bitmap*);
	static constexpr u64 NUM_COLORS = sizeof(Colors) / sizeof(D2D_COLOR_F);
	
	union {
		Icons icons = {};
		ID2D1Bitmap* iconArray[NUM_ICONS];
	};

	union {
		Colors colors = {};
		D2D_COLOR_F colorArray[NUM_COLORS];
	};
	
	Font fontUi = {};
	Font fontEditor = {};
		
	//-----------------------------------------------------
	// functions
	
	bool Init(const cJSON* jsonSettings, ID2D1DeviceContext* deviceContext);

	ID2D1Brush* GetAccentBrush(const D2D1_RECT_F* area = nullptr, f32 animationValue = 0.0f);
	
	ID2D1SolidColorBrush* GetBrushGlow();
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
	
	~Theme() noexcept;
};

extern Theme theme;