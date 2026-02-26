#pragma once
#include "basic.hh"

#include <string_view>
#include <vector>
#include <memory>

struct Font;
struct D2D_POINT_2F;
struct ID2D1RenderTarget;
struct ID2D1Brush;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct GlyphRunShapingMemory {
	u32* utf32Text = nullptr;
	s32* designGlyphAdvances = nullptr;
	u64 capacity = 0u;
	u64 size     = 0u;
	std::unique_ptr<u32[]> memory = nullptr;
};

struct GlyphRunBase {
	f32* advances = nullptr;
	u16* indicies = nullptr;

	u64 size = 0;
	u64 capacity = 0;
	std::unique_ptr<s8[]> memory = nullptr;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct GlyphRun : public GlyphRunBase {

	bool Shape(std::string_view text, const Font& font, GlyphRunShapingMemory* mem = nullptr);

	u64 Hittest(float offset) const;
	float GetGlyphOffset(u64 codepoint) const;
	void  GetGlyphOffsetRange(u64 from, u64 to, /*out*/ float* fromOffset, /*out*/ float* toOffset) const;

	float GetTotalAdvance() const;

	void Draw(ID2D1RenderTarget* renderTarget, const D2D_POINT_2F& pos, const Font& font, ID2D1Brush* textBrush) const;
	
	// @IDEA DrawCenter
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct GlyphRunMultiline : public GlyphRunBase {
	
	std::vector<u64> linebreaks = {};
	
	bool Shape(std::string_view text, const Font& font, GlyphRunShapingMemory* mem = nullptr);
	void Draw(ID2D1RenderTarget* renderTarget, const D2D_POINT_2F& pos, const Font& font, ID2D1Brush* textBrush, f32 extraOffsetFirstLine = 0.0f) const;
	
	u64 GetLineCount() const;
	
	// get the total advance of the longest line
	f32 GetMaxAdvance(f32 extraOffsetFirstLine = 0.0f) const;
	// get the total advance of the first line
	f32 GetAdvanceFirst() const;
};
