#pragma once

struct D2D_RECT_F;
struct D2D_POINT_2F;
struct D2D_SIZE_F;
struct D2D1_ROUNDED_RECT;

// create a point
D2D_POINT_2F MakePoint(float x, float y);
// create a size
D2D_SIZE_F MakeSize(float w, float h);

// make a rect from the top left point and the size
D2D_RECT_F MakeRect(const D2D_POINT_2F tl, const D2D_SIZE_F size);
// make a rect from the top left point and the width and height
D2D_RECT_F MakeRect(float x, float y, float w, float h);

// check if the point is inside the rectangle
bool RectContains(const D2D_RECT_F& rect, const D2D_POINT_2F& pt);
bool RectContains(const D2D_RECT_F& rect, float x, float y);

// calc the width of the rectangle
float RectWidth(const D2D_RECT_F &rect);
// calc the height of the rectangle
float RectHeight(const D2D_RECT_F &rect);
// calc the size of the rectangle
D2D_SIZE_F RectSize(const D2D_RECT_F &rect);

// check if 2 rectangles are equal
bool RectEquals(const D2D_RECT_F& lhs, const D2D_RECT_F& rhs);

// make a rounded rect from the top left point and the size and radius
D2D1_ROUNDED_RECT MakeRoundedRect(const D2D_POINT_2F tl, const D2D_SIZE_F size, float radius);
// make a rounded rect from the top left point and the width and height and radius
D2D1_ROUNDED_RECT MakeRoundedRect(float x, float y, float w, float h, float radius);
// make a rounded rect from a normal rect
D2D1_ROUNDED_RECT MakeRoundedRect(const D2D_RECT_F& rect, float radius);