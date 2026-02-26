#pragma once
#include "graphics/glyph-run.hh"

struct LayoutManager;

struct StatusBar {
	
	//-----------------------------------------
	// types
	
	enum ElementType {
		 ElementType_None = 0,
		 ElementType_Padding,
		 ElementType_ExplorerButton,
		 ElementType_ConsoleButton,
		 ElementType_ConsoleProgress,
		 ElementType_LanguageSelector,
		 ElementType_Diagnostics,
		 ElementType_CaretInfo,
		 ElementType_EncodingSelector,
		 ElementType_LineEndingSelector,
		 ElementType_MAX
	};
	
	//-----------------------------------------
	// data	
		
	GlyphRunShapingMemory shapingMemory = {};
	
	std::vector<ElementType> elements = {};
	u64 l2rElementCount = 0u;
		
	bool Init();
	void OnUpdate();
};

static void SetStatusBarText(std::string_view text);