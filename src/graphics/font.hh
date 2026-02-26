#pragma once
#include "basic.hh"
#include <string_view>
#include <memory>

struct IDWriteFontFace1;

struct FontMeasureTextMemory {
	u32* utf32Text     = nullptr;
	s32* glyphAdvances = nullptr;
	u16* glyphIndicies = nullptr;
	
	u64 capacity = 0u;
	u64 size     = 0u;
	std::unique_ptr<s8[]> memory = {};
};

struct Font {
	
	//-----------------------------------------------------
	// types

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

		std::string_view name = {};
		f32 size = .0f;

		s32 weight  = Weight_Normal;
		s32 style   = Style_Normal;
		s32 stretch = Stretch_Normal;
	};

	//-----------------------------------------------------
	// data

	IDWriteFontFace1* fontFace  = nullptr;
	f32 size = .0f;

	s16 designUnitsPerEm = 0;
	u16 glyphIndexSpace = 0;

	f32 lineHeight = .0f;
	f32 baselineOffset = .0f;
	f32 underlineOffset = .0f;

	//-----------------------------------------------------
	// functions

	bool Init(const Description& desc);

	void ChangeFontSize(f32 newFontSize);

	float GetSpaceAdvance() const;
	float ConvertFromDesignUnits(s32 designUnits) const;

	float MeasureText(std::string_view text, FontMeasureTextMemory* mem = nullptr) const;

	~Font() noexcept;
};
