#pragma once

struct _D3DCOLORVALUE;
namespace toml { class node; };

union Color {
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	struct {
		float r;
		float g;
		float b;
		float a;
	};
	float array[4];
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	static Color FromKnown(int d2dIndex);
	static bool FromToml(const toml::node& node, /*out*/ Color* color);
	_D3DCOLORVALUE ToD2D() const;
	
	Color(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) noexcept;
	
	//-----------------------------------------------------
	// Constants
	//-----------------------------------------------------
};

extern const Color COLOR_TRANSPARENT;
extern const Color COLOR_RED;
extern const Color COLOR_GREEN;
extern const Color COLOR_BLUE;
extern const Color COLOR_CYAN;
extern const Color COLOR_YELLOW;
extern const Color COLOR_MAGENTA;
extern const Color COLOR_WHITE;
extern const Color COLOR_BLACK;