#pragma once
#include "basic.hh"

#include <memory>
#include <string_view>
#include <span>
#include <vector>

struct TextBuffer;
struct Font;
struct ID2D1RenderTarget;
struct ID2D1SolidColorBrush;
struct D2D_POINT_2F;

struct GlyphRun {
	
	//------------------------------------------
	// data
	//------------------------------------------
	
	std::unique_ptr<f32[]> glyphAdvances = nullptr;
	u16* glyphIndicies = nullptr;
	std::unique_ptr<u32[]> charMapping = nullptr;
	
	u32  glyphCount     = 0u;
	u32  glyphCapacity  = 0u;
	u32  charCount      = 0u;
	f32  width          = 0.0f;
	
	//------------------------------------------
	// functions
	//------------------------------------------	

	static bool ShapeBatch(std::span<const std::string_view> batch, const Font& font, /*out*/ std::vector<GlyphRun>* runs);
	static bool ShapeBatch(const TextBuffer& textBuffer , const Font& font, /*out*/ std::vector<GlyphRun>* runs);
		
	bool Shape(std::string_view text, const Font& font);
		
	// draw full run
	void Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const;
	
	// @TODO implement
	// draw centered in the given width
	void DrawCenter(ID2D1RenderTarget* renderTarget, f32 x, f32 y, f32 availableW, const Font& font, ID2D1SolidColorBrush* brush) const;	
	
	// draw only a part of the glyph run
	// startChar is the index in the original char, not the index of the glyph!
	void DrawPartial(ID2D1RenderTarget* renderTarget, f32 x, f32 y, u64 startChar, u64 amount, const Font& font, ID2D1SolidColorBrush* brush, /*out*/ f32* drawWidth = nullptr) const;
	
	// shape and then immediatly draw
	bool ShapeAndDraw(ID2D1RenderTarget* renderTarget, std::string_view text, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush);
	
	u64  HitTest(f32 offset) const;
	f32  MeasureOffset(u64 pos) const;
	void MeasureOffsetRange(u64 startChar, u64 endChar, f32* offFrom, f32* offTo) const;
};

struct GlyphRunMultiline {
	GlyphRun glyphRun = {};
	std::vector<u32> lineEnds = {};
	
	bool Shape(std::string_view text, const Font& font);
	void Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const;
	
	f32 GetWidth() const;
	u64 LineCount() const; 
};

bool InitStaticShapingBuffer();
extern GlyphRun staticGlyphRun;
