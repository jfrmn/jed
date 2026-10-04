#pragma once
#include "events.hh"
#include "commands.hh"
#include "util/color.hh"
#include "util/hashtable.hh"
#include "glyph-run.hh"

#include <unordered_map>

struct ID2D1Bitmap;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;
struct Tool;

struct Settings {
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
	
	struct Colors {
		Color unknown              = {1.0f, 0.0f, 0.1f, 1.0f};
		Color dropShadow           = {0.2f, 0.3f, 0.6f, 1.0f}; // @TODO was "glow" - don't forget to change settings.json!!
		Color activePanelFrame     = {0.2f, 0.3f, 0.6f, 1.0f};
		Color selection            = {0.0f, 1.0f, 1.0f, 0.3f};
		Color selectionInactive    = {1.0f, 1.0f, 1.0f, 0.3f};
		Color hover                = {1.0f, 1.0f, 1.0f, 0.5f};
		Color hoverPressed         = {0.8f, 0.8f, 0.8f, 0.5f};
		Color toggled              = {0.8f, 0.8f, 0.8f, 0.5f};
		Color editorText           = {1.0f, 1.0f, 1.0f, 1.0f};
		Color editorBackground     = {0.1f, 0.1f, 0.1f, 1.0f};
		Color editorMultiCaret     = {1.0f, 0.0f, 1.0f, 1.0f};
		Color uiText               = {1.0f, 1.0f, 1.0f, 1.0f};
		Color uiTextInactive       = {0.6f, 0.6f, 0.6f, 1.0f};
		Color uiSearchResult       = {1.0f, 1.0f, 0.0f, 0.3f};
		Color uiBackground         = {0.3f, 0.3f, 0.3f, 1.0f};
		Color uiBackgroundInactive = {0.2f, 0.2f, 0.2f, 1.0f};
		Color uiBackgroundInvalid  = {0.4f, 0.0f, 0.0f, 1.0f};
		
		ID2D1SolidColorBrush* UseDropShadow() const;
		ID2D1SolidColorBrush* UseSelection(bool active = true) const;
		ID2D1SolidColorBrush* UseHover(bool pressed = false) const;
		ID2D1SolidColorBrush* UseToggle() const;
		ID2D1SolidColorBrush* UseEditorText() const;
		ID2D1SolidColorBrush* UseEditorBackground() const;
		ID2D1SolidColorBrush* UseEditorMultiCaret() const;
		ID2D1SolidColorBrush* UseUiText(bool active = true) const;
		ID2D1SolidColorBrush* UseSearchResult(bool active = true) const;
		ID2D1SolidColorBrush* UseUiBackground(bool active = true) const;
		ID2D1SolidColorBrush* UseUiBackgroundInvalid() const;
	};
	
	struct KeyBind {
		Command::Id commandId = Command::Id_None;
		std::vector<ParameterValue> parameters = {};
	};
	
	//-----------------------------------------------------
	// constants
	//-----------------------------------------------------
	
	static constexpr u64 NUM_COLORS  = sizeof(Colors) / sizeof(Color);

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	union {
		Colors colors = {};
		Color colorArray[NUM_COLORS];
	};
	
	std::unordered_map<u64, KeyBind> keyBinds = {};
	Hashtable<Color> syntaxColors = {};	
	std::vector<Tool> tools = {};
		
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
		
	bool LookupKeyBind(const Event& event, /*out*/ Command* command);
};

extern Settings settings;
