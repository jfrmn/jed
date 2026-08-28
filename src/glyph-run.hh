#pragma once
#include "basic.hh"

#include <memory>
#include <string>
#include <string_view>
#include <span>
#include <vector>

struct TextBuffer;
struct Font;
struct ID2D1RenderTarget;
struct ID2D1SolidColorBrush;
struct D2D_POINT_2F;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Font
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Font {
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	enum Weight {
		 Weight_Light = 300,
		 Weight_Semi_Leight = 350,
		 Weight_Normal = 400,
		 Weight_Semi_Bold = 600,
		 Weight_Bold = 700
	};

	enum Style {
		 Style_Normal = 0,
		 Style_Oblique,
		 Style_Italic
	};

	enum Stretch {
		 Stretch_Normal = 5,
		 Stretch_Expanded = 7,
		 Stretch_Condensed = 3
	};

	struct Description {

		std::string name = {};
		f32 size = .0f;

		s64 weight  = Weight_Normal;
		s64 style   = Style_Normal;
		s64 stretch = Stretch_Normal;
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	struct IDWriteFontFace1* fontFace  = nullptr;
	f32 size = .0f;

	s16 designUnitsPerEm = 0;
	u16 glyphIndexSpace = 0;
	
	f32 spaceAdvance = .0f;
	f32 lineHeight = .0f;
	f32 baselineOffset = .0f;
	f32 underlineOffset = .0f;
	f32 strikethroughOffset = .0f;

	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	bool Init(const Description& desc);

	void ChangeFontSize(f32 newFontSize);
	f32 ConvertFromDesignUnits(s32 designUnits) const;

	~Font() noexcept;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Glyph run
//
///////////////////////////////////////////////////////////////////////////////////////////////////

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

	static bool ShapeBatch(std::span<const std::string_view> batch, const Font& font, /*out*/ std::span<GlyphRun> output);
	static bool ShapeBatch(const TextBuffer& textBuffer , const Font& font, /*out*/ std::span<GlyphRun> output);
		
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
