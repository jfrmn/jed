#pragma once
#include "basic.hh"

#include <memory>
#include <string_view>
#include <span>
#include <vector>

struct Font;
struct ID2D1RenderTarget;
struct ID2D1SolidColorBrush;

struct GlyphRun_DWrite {
	
	//------------------------------------------
	// data
	//------------------------------------------
	
	std::unique_ptr<f32[]> glyphAdvances = nullptr;
	u16* glyphIndicies = nullptr;
	std::unique_ptr<u32[]> charMapping = nullptr;
	
	u32  glyphCount    = 0u;
	u32  glyphCapacity = 0u;
	u32  charCount     = 0u;
	f32  width         = 0.0f;
	
	//------------------------------------------
	// functions
	//------------------------------------------	

	static bool ShapeBatch(std::span<const std::string_view> batch, const Font& font, /*out*/ std::vector<GlyphRun_DWrite>* runs);
		
	bool Shape(std::string_view text, const Font& font);
	
	u64  HitTest(f32 offset) const;
	f32  MeasureOffset(u64 pos) const;
	void MeasureOffsetRange(u64 from, u64 to, f32* offFrom, f32* offTo) const;
	
	void Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const;
	void DrawCenter(ID2D1RenderTarget* renderTarget, f32 x, f32 y, f32 availableW, const Font& font, ID2D1SolidColorBrush* brush) const;
};

extern bool InitStaticTextAnalyzer();extern void ShutdownStaticTextAnalyzer();