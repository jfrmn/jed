#pragma once
#include "basic.hh"
#include <string>

struct IDWriteFontFace1;

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

		std::string name = {};
		f32 size = .0f;

		s64 weight  = Weight_Normal;
		s64 style   = Style_Normal;
		s64 stretch = Stretch_Normal;
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
	f32 strikethroughOffset = .0f;

	//-----------------------------------------------------
	// functions

	bool Init(const Description& desc);

	void ChangeFontSize(f32 newFontSize);

	float GetSpaceAdvance() const;
	float ConvertFromDesignUnits(s32 designUnits) const;

	~Font() noexcept;
};
