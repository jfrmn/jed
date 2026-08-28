#pragma once
#include "text/text-buffer.hh"
#include "glyph-run.hh"

#include <vector>

struct D2D_RECT_F;

struct FilePreview {
	
	//-------------------------------------------
	// types

	enum LoadMode {
		 LoadMode_Unknown = 0,
		 LoadMode_FirstFewLine,
		 LoadMode_TargetLine,
		 LoadMode_LineRange
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
		
		bool hasSelection = false;
		TextPosition selectionFrom = {};
		TextPosition selectionTo = {};
	};

	//-------------------------------------------
	// data
		
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 width = 0.0f;
	
	TextBuffer textBuffer = {};
	std::vector<GlyphRun> lines = {};
	
	bool hasError = false;
	bool hasSelection = false;
	
	// line-member is adjusted to the displayed range
	TextPosition selectionFrom = {};
	TextPosition selectionTo = {};
		
	//-------------------------------------------
	// functions
	
	void Init();	
	bool Load(const LoadArgs& args);
	
	D2D_RECT_F GetArea() const;
	
	void OnUpdate();
};
