#include "color.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d2d1.h>

const Color COLOR_TRANSPARENT {0.0f, 0.0f, 0.0f, 0.0f};
const Color COLOR_RED         {1.0f, 0.0f, 0.0f, 1.0f};
const Color COLOR_GREEN       {0.0f, 1.0f, 0.0f, 1.0f};
const Color COLOR_BLUE        {0.0f, 0.0f, 1.0f, 1.0f};
const Color COLOR_WHITE       {1.0f, 1.0f, 1.0f, 1.0f};
const Color COLOR_BLACK       {0.0f, 0.0f, 0.0f, 1.0f};

Color Color::FromKnown(int d2dIndex) {
	const auto enumVal = static_cast<D2D1::ColorF::Enum>(d2dIndex);
	const D2D_COLOR_F d2dColor = D2D1::ColorF(enumVal);
	return Color {d2dColor.r, d2dColor.g, d2dColor.b, d2dColor.a};
}

_D3DCOLORVALUE Color::ToD2D() const {
	return D2D_COLOR_F {
		.r = this->r,
		.g = this->g,
		.b = this->b,
		.a = this->a};
}
