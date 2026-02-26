#pragma once
#include "graphics/font.hh"

#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

struct cJSON;

struct Style {
	
	//-----------------------------------------------------
	// types
	
	enum Icon {
		 Icon_Unknown = 0,
		 Icon_Waiting,
		 Icon_Error,
		 Icon_NoItems,
		 
		 Icon_Tabs_ModifiedHovered,
		 Icon_Tabs_Modified,
		 Icon_Tabs_Hovered,
	
		 Icon_EditorDiagnostics_Error,
		 Icon_EditorDiagnostics_Warning,
		 Icon_EditorDiagnostics_Info,
		 Icon_EditorDiagnostics_Hint,

		 Icon_EditorAutocomplete_Text,
		 Icon_EditorAutocomplete_Method,
		 Icon_EditorAutocomplete_Function,
		 Icon_EditorAutocomplete_Constructor,
		 Icon_EditorAutocomplete_Field,
		 Icon_EditorAutocomplete_Variable,
		 Icon_EditorAutocomplete_Class,
		 Icon_EditorAutocomplete_Interface,
		 Icon_EditorAutocomplete_Module,
		 Icon_EditorAutocomplete_Property,
		 Icon_EditorAutocomplete_Unit,
		 Icon_EditorAutocomplete_Value,
		 Icon_EditorAutocomplete_Enum,
		 Icon_EditorAutocomplete_Keyword,
		 Icon_EditorAutocomplete_Snippet,
		 Icon_EditorAutocomplete_Color,
		 Icon_EditorAutocomplete_File,
		 Icon_EditorAutocomplete_Reference,
		 Icon_EditorAutocomplete_Folder,
		 Icon_EditorAutocomplete_EnumMember,
		 Icon_EditorAutocomplete_Constant,
		 Icon_EditorAutocomplete_Struct,
		 Icon_EditorAutocomplete_Event,
		 Icon_EditorAutocomplete_Operator,
		 Icon_EditorAutocomplete_TypeParameter,

		 Icon_EditorSearch_Resultsclosed,
		 Icon_EditorSearch_Resultsopened,
		 
		 Icon_Explorer_FolderOpen,
		 Icon_Explorer_FolderClosed,
		 Icon_Explorer_File,
		 
		 Icon_Lsp_Standby,
		 Icon_Lsp_Initializing,
		 Icon_Lsp_Running,
		 Icon_Lsp_ShuttingDown,
		 Icon_Lsp_Exited,
		 Icon_Lsp_Crashed,

		 Icon_MAX
	};
	
	enum Color {
		 Color_Unknown = 0,
		 Color_Glow,
		 Color_ActivePanelFrame,
		 Color_Selection,
		 Color_SelectionInactive,
		 Color_Hover,
		 Color_Pressed,
		 Color_Toggled,
		 Color_EditorText,
		 Color_EditorBackground,
		 Color_EditorMultiCaretEdit,
		 Color_UiText,
		 Color_UiTextInactive,
		 Color_UiSearchResult,
		 Color_UiBackground,
		 Color_UiBackgroundInactive,
		 Color_UiBackgroundInvalid,
		 
		 Color_MAX
	};
	
	enum BrushType {
		 BrushType_None = 0,
		 BrushType_SolidColor,
		 BrushType_LinearGradient,
		 BrushType_RadialGradient
	};
	
	struct BrushDescription {
		BrushType type = BrushType_None;
		D2D1_COLOR_F color = {};
		bool repeatMirrored = false;
		std::vector<D2D1_GRADIENT_STOP> stops = {};
		D2D1_EXTEND_MODE extendMode = D2D1_EXTEND_MODE_CLAMP;
	};
	
	//-----------------------------------------------------
	// data
	
	D2D1_COLOR_F colors[Color_MAX] = {};
	
	BrushDescription accentBrushDescription = {};
	ID2D1Brush* accentBrush = nullptr;

	Font fontUi = {};
	Font fontEditor = {};
	
	ID2D1SolidColorBrush* brush = nullptr;
	ID2D1Bitmap* icons[Icon_MAX] = {};
	
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
	
	~Style() noexcept;
};

extern Style style;