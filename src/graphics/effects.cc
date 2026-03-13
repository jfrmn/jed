#include "effects.hh"
#include "theme.hh"
#include "util/logging.hh"
#include "util/rect-util.hh"
#include "ui/constants.h"
#include "graphics/factories.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static const GUID guidGaussianBlurEffect { 0x1feb6d69, 0x2fe6, 0x4ac9, { 0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5 } };
static const GUID guidShadowEffect       { 0xC67EA361, 0x1863, 0x4e69, { 0x89, 0xDB, 0x69, 0x5D, 0x3E, 0x9A, 0x5B, 0x6B } };
static const GUID guidBlendEffect        { 0xc80ecff0, 0x3fd5, 0x4f05, { 0x83, 0x28, 0xc5, 0xd1, 0x72, 0x4b, 0x4f, 0x0a } };

static constexpr f32 STANDARD_DEVIATION = 10.0f;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1Effect* blurEffect = nullptr;
ID2D1Effect* shadowEffect = nullptr;
ID2D1Effect* blendEffect = nullptr;

// We use this brush to define the alpha mask (aka draw the text) when using the BlendImages-function.
// It is a completely opaque and solid white brush.
// Even though the brush will be used on an bitmap-rendertarget and was create on the deviceContext this will still work,
// because we use CreateCompatibleRenderTarget.
// See: https://learn.microsoft.com/en-us/windows/win32/direct2d/resources-and-resource-domains#compatible-render-targets-and-shared-bitmaps
ID2D1SolidColorBrush* alphaMaskBrush = nullptr;

// global brush to use everywhere
ID2D1SolidColorBrush* brush = nullptr;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool InitEffects(ID2D1DeviceContext* deviceContext) {
	
	if (HRESULT hr = deviceContext->CreateEffect(guidGaussianBlurEffect, &blurEffect); hr != S_OK) {
		LogError("CreateEffect() failed for blur-effect, HRESULT: %", FHr(hr));
		return false;
	}
	
	blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, STANDARD_DEVIATION);
	blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_SHADOW_OPTIMIZATION_SPEED);
	blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
		
	if (HRESULT hr = deviceContext->CreateEffect(guidShadowEffect, &shadowEffect); hr != S_OK) {
		LogError("CreateEffect() failed for shadow-effect, HRESULT: %", FHr(hr));
		return false;
	}
	
	shadowEffect->SetValue(D2D1_SHADOW_PROP_OPTIMIZATION, D2D1_SHADOW_OPTIMIZATION_SPEED);
	shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, STANDARD_DEVIATION);
	
	if (HRESULT hr = deviceContext->CreateEffect(guidBlendEffect, &blendEffect); hr != S_OK) {
		LogError("CreateEffect() failed for blend-effect, HRESULT: %", FHr(hr));
		return false;
	}
	
	blendEffect->SetValue(D2D1_BLEND_PROP_MODE, D2D1_BLEND_MODE_MULTIPLY);
	
	if (HRESULT hr = deviceContext->CreateSolidColorBrush(D2D_COLOR_F {0.0f, 0.0f, 0.0f, 1.0f}, &brush); hr != S_OK) {
		LogError("CreateSolidColorBrush() failed for global brush. HRESULT: %", FHr(hr));
		return false;
	}

	if (HRESULT hr = deviceContext->CreateSolidColorBrush(D2D_COLOR_F {1.0f, 1.0f, 1.0f, 1.0f}, &alphaMaskBrush); hr != S_OK) {
		LogError("CreateSolidColorBrush() failed for alpha-mask-brush. HRESULT: %", FHr(hr));
		return false;
	}
	
	return true;
}

void ShutdownEffects() {
	
	if (blurEffect) {
		blurEffect->Release();
		blurEffect = nullptr;
	}
	
	if (shadowEffect) {
		shadowEffect->Release();
		shadowEffect = nullptr;
	}
	
	if (blendEffect) {
		blendEffect->Release();
		blendEffect = nullptr;
	}
	
	if (alphaMaskBrush) {
		alphaMaskBrush->Release();
		alphaMaskBrush = nullptr;
	}

	if (brush) {
		brush->Release();
		brush = nullptr;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1Bitmap* CopyFromRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area) {

	float dpiX, dpiY;
	deviceContext->GetDpi(&dpiX, &dpiY);

	ID2D1Bitmap* bitmap = nullptr;
	if (HRESULT hr = deviceContext->CreateBitmap(
			D2D1_SIZE_U {
				.width = static_cast<UINT32>(RectWidth(area)),
				.height = static_cast<UINT32>(RectHeight(area))},
			D2D1_BITMAP_PROPERTIES {
				.pixelFormat = deviceContext->GetPixelFormat(),
				.dpiX = dpiX,
				.dpiY = dpiY},
			&bitmap); hr != S_OK) {
		LogError("CreateBitmap() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	const D2D1_POINT_2U copyDestination {0u, 0u};
	const D2D1_RECT_U copySourceRect {
		.left   = static_cast<UINT32>(area.left),
		.top    = static_cast<UINT32>(area.top),
		.right  = static_cast<UINT32>(area.right),
		.bottom = static_cast<UINT32>(area.bottom) };
	
	if (HRESULT hr = bitmap->CopyFromRenderTarget(&copyDestination, deviceContext, &copySourceRect); hr != S_OK) {
		LogError("CopyFromRenderTarget failed. HRESULT: %", FHr(hr));
		bitmap->Release();
		return nullptr;
	}
	
	return bitmap;	
}

void BlurArea(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& area, ID2D1Bitmap* background /*= nullptr*/) {
	
	if (!background) {
		background = CopyFromRenderTarget(deviceContext, area);
		if (!background) return;
	} else {
		background->AddRef();
	}
		
	blurEffect->SetInput(0u, background);
	deviceContext->DrawImage(blurEffect, {area.left, area.top});
	
	background->Release();
}

void DrawGlow(ID2D1DeviceContext* deviceContext, ID2D1Bitmap* background, const D2D_RECT_F& area) {
	shadowEffect->SetInput(0, background);
	shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1_VECTOR_4F {theme.colors.dropShadow.r, theme.colors.dropShadow.g, theme.colors.dropShadow.b, theme.colors.dropShadow.a});
	deviceContext->DrawImage(shadowEffect, D2D1_POINT_2F {area.left, area.top});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1BitmapRenderTarget* CreateCompatibleRenderTarget(ID2D1DeviceContext* deviceContext, const D2D_SIZE_F& size) {
	ID2D1BitmapRenderTarget* renderTarget = nullptr;
	if (HRESULT hr = deviceContext->CreateCompatibleRenderTarget(size, &renderTarget); hr != S_OK) {
		LogError("CreateCompatibleRenderTarget() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	return renderTarget;
}

void BlendImages(ID2D1DeviceContext* deviceContext, const D2D_POINT_2F& pos, ID2D1Bitmap* first, ID2D1Bitmap* second) {
	blendEffect->SetInput(0, first);
	blendEffect->SetInput(1, second);
	
	deviceContext->DrawImage(blendEffect, D2D_POINT_2F {std::floor(pos.x), std::floor(pos.y)});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void PushLayer(ID2D1DeviceContext* deviceContext, const D2D_RECT_F& boundingBox) {
	PushLayer(deviceContext, D2D1_ROUNDED_RECT {
		.rect = boundingBox,
		.radiusX = RADIUS,
		.radiusY = RADIUS});
}
void PushLayer(ID2D1DeviceContext* deviceContext, const D2D1_ROUNDED_RECT& boundingBox) {
	ID2D1RoundedRectangleGeometry* geometry = nullptr;
	if (HRESULT hr = d2dFactory->CreateRoundedRectangleGeometry(boundingBox, &geometry); hr != S_OK) {
		LogError("CreateRoundedRectangleGeometry() failed. HRESULT: %", FHr(hr));
		return;
	}
			
	deviceContext->PushLayer(
		D2D1::LayerParameters(
			D2D1::InfiniteRect(),
			geometry,
			D2D1_ANTIALIAS_MODE_ALIASED),
		nullptr);
		
	geometry->Release();
}

void PopLayer(ID2D1DeviceContext* deviceContext) {
	deviceContext->PopLayer();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1SolidColorBrush* GetBrush(const _D3DCOLORVALUE& clr) {
	brush->SetColor(clr);
	return brush;
}
