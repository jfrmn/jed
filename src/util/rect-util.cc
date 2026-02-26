#include "rect-util.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

D2D_POINT_2F MakePoint(float x, float y) {
	return D2D_POINT_2F {
		.x = x,
		.y = y };
}

D2D_SIZE_F MakeSize(float w, float h) {
	return D2D_SIZE_F {
		.width = w,
		.height = h };
}

D2D_RECT_F MakeRect(const D2D_POINT_2F tl, const D2D_SIZE_F size) {
	return D2D_RECT_F {
		.left   = tl.x,
		.top    = tl.y,
		.right  = tl.x + size.width,
		.bottom = tl.y + size.height };
}

D2D_RECT_F MakeRect(float x, float y, float w, float h) {
	return D2D_RECT_F {
		.left   = x,
		.top    = y,
		.right  = x + w,
		.bottom = y + h };
}

bool RectContains(const D2D_RECT_F& rect, const D2D_POINT_2F& pt) {
	return RectContains(rect, pt.x, pt.y);
}

bool RectContains(const D2D_RECT_F& rect, float x, float y) {
	return rect.left   < x &&
		   rect.top    < y &&
		   rect.right  > x &&
		   rect.bottom > y;
}

D2D_RECT_F RectContains(float x, float y, float w, float h) {
	return D2D_RECT_F {
		.left   = x,
		.top    = y,
		.right  = x + w,
		.bottom = y + h };
}


float RectWidth(const D2D_RECT_F& rect) {
	return rect.right - rect.left;
}

float RectHeight(const D2D_RECT_F& rect) {
	return rect.bottom - rect.top;
}

D2D_SIZE_F RectSize(const D2D_RECT_F &rect) {
	return D2D_SIZE_F {
		.width = rect.right - rect.left,
		.height = rect.bottom - rect.top };
}

bool RectEquals(const D2D_RECT_F& lhs, const D2D_RECT_F& rhs) {
	return lhs.left   == rhs.left &&
	       lhs.top    == rhs.top &&
	       lhs.right  == rhs.right &&
	       lhs.bottom == rhs.bottom;
}


D2D_POINT_2F TopLeft(const D2D_RECT_F& rect) {
	return D2D_POINT_2F {
		.x = rect.left,
		.y = rect.top };
}

D2D_POINT_2F BottomRight(const D2D_RECT_F& rect) {
	return D2D_POINT_2F {
		.x = rect.right,
		.y = rect.bottom };
}

bool Contains(const D2D_RECT_F &rect, float x, float y) {
	return (rect.left   < x)
		&& (rect.right  > x)
		&& (rect.top    < y)
		&& (rect.bottom > y);
}

D2D1_ROUNDED_RECT MakeRoundedRect(const D2D_POINT_2F tl, const D2D_SIZE_F size, float radius) {
	return D2D1_ROUNDED_RECT {
		.rect = MakeRect(tl, size),
		.radiusX = radius,
		.radiusY = radius };
}

D2D1_ROUNDED_RECT MakeRoundedRect(float x, float y, float w, float h, float radius) {
	return D2D1_ROUNDED_RECT {
		.rect = MakeRect(x, y, w, h),
		.radiusX = radius,
		.radiusY = radius };
}

D2D1_ROUNDED_RECT MakeRoundedRect(const D2D_RECT_F &rect, float radius) {
	return D2D1_ROUNDED_RECT {
		.rect = rect,
		.radiusX = radius,
		.radiusY = radius };
}
