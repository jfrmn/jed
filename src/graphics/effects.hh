#pragma once

struct ID2D1Effect;
struct ID2D1DeviceContext;
struct ID2D1Bitmap;
struct ID2D1SolidColorBrush;
struct ID2D1BitmapRenderTarget;

struct D2D_RECT_F;
struct D2D_POINT_2F;
struct D2D_SIZE_F;
struct D2D1_ROUNDED_RECT;

union Color;

//-----------------------------------------------------------------------------
// Init & Shutdown
//-----------------------------------------------------------------------------
bool InitEffects(ID2D1DeviceContext* deviceContext);
void ShutdownEffects();

//-----------------------------------------------------------------------------
// Bluring
//-----------------------------------------------------------------------------
ID2D1Bitmap* CopyFromRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area);
void BlurArea(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area, ID2D1Bitmap* background = nullptr);

void DrawGlow(ID2D1DeviceContext* deviceContext, ID2D1Bitmap* background, const D2D_RECT_F& area);

//-----------------------------------------------------------------------------
// Blending
//-----------------------------------------------------------------------------
ID2D1BitmapRenderTarget* CreateCompatibleRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_SIZE_F& size);
void BlendImages(ID2D1DeviceContext* deviceContext, const D2D_POINT_2F& pos, ID2D1Bitmap* first, ID2D1Bitmap* second);

extern ID2D1SolidColorBrush* alphaMaskBrush;

//-----------------------------------------------------------------------------
// Layers
//-----------------------------------------------------------------------------
void PushLayer(ID2D1DeviceContext* deviceContext, const D2D1_ROUNDED_RECT& area);
void PushLayer(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area);
void PopLayer(ID2D1DeviceContext* deviceContext);

//-----------------------------------------------------------------------------
// global Brush
//-----------------------------------------------------------------------------
ID2D1SolidColorBrush* GetBrush(const Color& clr);

extern ID2D1SolidColorBrush* brush;
