#pragma once

struct ID2D1Factory;
struct IDWriteFactory;
struct IWICImagingFactory;
struct ID2D1DeviceContext;
struct ID2D1SolidColorBrush;
struct ID2D1Bitmap;
struct ID2D1BitmapRenderTarget;
struct D2D_RECT_F;
struct D2D_POINT_2F;
struct D2D_SIZE_F;
union Color;


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Factories
//
///////////////////////////////////////////////////////////////////////////////////////////////////

extern ID2D1Factory* d2dFactory;
extern IDWriteFactory* dwFactory;
extern IWICImagingFactory* wicFactory;

bool InitFactories();
void ShutdownFactories();

bool InitGraphics(ID2D1DeviceContext* deviceContext);
void ShutdownGraphics();

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Render functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// actice device context
extern ID2D1DeviceContext* deviceContext;

//-----------------------------------------------------
// effects
//-----------------------------------------------------

// copy from render target
ID2D1Bitmap* CopyFromRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area);

// blur area. if background is null CopyFromRenderTarget() will be called
void BlurArea(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area, ID2D1Bitmap* background = nullptr);

// draw glow/drop shadow. unlike BlurArea the background must be provided
void DrawGlow(ID2D1DeviceContext* deviceContext, ID2D1Bitmap* background, const D2D_RECT_F& area);

ID2D1BitmapRenderTarget* CreateCompatibleRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_SIZE_F& size);
void BlendImages(ID2D1DeviceContext* deviceContext, const D2D_POINT_2F& pos, ID2D1Bitmap* first, ID2D1Bitmap* second);

// We use this brush to define the alpha mask (aka the drawn text) when rendering colored text via the BlendImage()-function.
// It is a completely opaque and solid white brush.
// Even though the brush will be used on an bitmap-rendertarget and was create on the deviceContext this will still work,
// because we use CreateCompatibleRenderTarget.
// See: https://learn.microsoft.com/en-us/windows/win32/direct2d/resources-and-resource-domains#compatible-render-targets-and-shared-bitmaps
extern ID2D1SolidColorBrush* alphaMaskBrush;

//-----------------------------------------------------
// layers
//-----------------------------------------------------

// push a rounded rect mask
void PushLayer(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area);
void PopLayer(ID2D1DeviceContext* deviceContext);

//-----------------------------------------------------
// global brush
//-----------------------------------------------------

// global brush to use everywhere
extern ID2D1SolidColorBrush* brush;

// set the color of the global brush and return that brush
ID2D1SolidColorBrush* UseColor(const Color& clr);


