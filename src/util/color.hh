#pragma once

struct _D3DCOLORVALUE;
namespace toml { class node; };

union Color {
	
	//-----------------------------------------------------
	// data
	
	float array[4] {0.0f, 0.0f, 0.0f, 1.0f};
	struct {
		float r;
		float g;
		float b;
		float a;
	};
	
	//-----------------------------------------------------
	// functions
	
	static Color FromKnown(int d2dIndex);
	static bool FromToml(const toml::node& node, /*out*/ Color* color);
	_D3DCOLORVALUE ToD2D() const;
};

extern const Color COLOR_TRANSPARENT;
extern const Color COLOR_RED;
extern const Color COLOR_GREEN;
extern const Color COLOR_BLUE;
extern const Color COLOR_WHITE;
extern const Color COLOR_BLACK;

