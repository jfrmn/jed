#pragma once
#include "text/text-buffer.hh"
#include "glyph-run.hh"
#include "util/color.hh"

#include <vector>

struct D2D_RECT_F;

struct FilePreview {
	
	//-------------------------------------------
	// types
	//-------------------------------------------

	enum LoadMode {
		 LoadMode_Unknown = 0,
		 LoadMode_FirstFewLine,
		 LoadMode_TargetLine,
		 LoadMode_LineRange
	};
	
	enum HighlightMode {
		 HighlightMode_None = 0,
		 HighlightMode_Selection,
		 HighlightMode_Underline
	};
	
	struct LoadArgs {
		std::string_view path = {};
		
		LoadMode mode = LoadMode_Unknown;
		union {
			u64 lineCount;
			u64 targetLine;
			struct {
				u64 lineFrom;
				u64 lineTo;
			};
		};
		
		HighlightMode highlightMode = HighlightMode_None;
		// could make this a union but msvc can't handle it.
		// see comment event.hh:80
		u64 underlinedLine = 0u;
		TextPosition selectionFrom = {};
		TextPosition selectionTo = {};
	};

	//-------------------------------------------
	// data
	//-------------------------------------------
		
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 width = 0.0f;
		
	Color color = {}; // color of glow and, if highlightMode = Underline, the color of the underline
	
	TextBuffer textBuffer = {};
	std::vector<GlyphRun> lines = {};
	
	bool hasError = false;
	
	// lines are adjusted to the displayed range
	HighlightMode highlightMode = HighlightMode_None;
	TextPosition  highlightFrom = {};
	TextPosition  highlightTo = {};
	
	//-------------------------------------------
	// functions
	//-------------------------------------------
	
	void Init();	
	bool Load(const LoadArgs& args);
	
	D2D_RECT_F GetArea() const;
	
	void OnUpdate();
};
