#include "icons.hh"
#include "logging.hh"
#include "graphics.hh"
#include "util.hh"
#include "settings.hh"

#include "util/color.hh"
#include "ui/constants.h"

#define WIN32_LEAN_AND_MEAN
#define MINMAX
#include <Windows.h>
#include <d2d1_1.h>
#include <wincodec.h>

Icons icons {};

D2D_RECT_F SourceRect(int iconId) {
	return MakeRect(static_cast<f32>(iconId * ICON_MAX_SIZE), 0.0f, ICON_MAX_SIZE, ICON_MAX_SIZE);
}

static bool AddIconToAtlas(ID2D1BitmapRenderTarget* bitmapRenderTarget, int iconId) {
		
	HRSRC hRes = FindResourceW(NULL, MAKEINTRESOURCEW(iconId), L"PNG");
	if (!hRes) {
		LogError("Failed to find resource with id %u. Last Error: %s", iconId, StrLastErr(GetLastError()));
		return false;
	}
		
	ID2D1Bitmap* bitmap = LoadBitmapFromResource(bitmapRenderTarget, hRes);
	if (!bitmap) {
		LogError("Failed to load bitmap of icon with id %u", iconId);
		return false;
	}
		
	const D2D_RECT_F dstRect = SourceRect(iconId);
	bitmapRenderTarget->FillOpacityMask(bitmap,
		UseColor(COLOR_WHITE),
		D2D1_OPACITY_MASK_CONTENT_GRAPHICS,
		&dstRect,
		nullptr);
	bitmap->Release();
	
	return true;
}

void Icons::DrawIcon(ID2D1DeviceContext* dc, int iconId, const D2D_POINT_2F& position, f32 widthAndHeight) {
	DrawIcon(dc, iconId, position, widthAndHeight, settings.colors.uiText);
}

void Icons::DrawIcon(ID2D1DeviceContext* dc, int iconId, const D2D_POINT_2F& position, f32 widthAndHeight, Color color) {
	ASSERT_SOFT(iconId >= 0 && iconId < ICON_COUNT);
	
	dc->FillOpacityMask(iconAtlas,
		UseColor(color),
		MakeRect(position.x, position.y, widthAndHeight, widthAndHeight),
		SourceRect(iconId));
}

bool Icons::Init(ID2D1DeviceContext* dc) {
	
	const D2D1_SIZE_F atlasSize {
		.width = ICON_MAX_SIZE * ICON_COUNT,
		.height = ICON_MAX_SIZE};
	
	const D2D1_PIXEL_FORMAT pixelFormat {
		.format = DXGI_FORMAT_A8_UNORM,
		.alphaMode = D2D1_ALPHA_MODE_STRAIGHT};
	
	ID2D1BitmapRenderTarget* bitmapRenderTarget = nullptr;
	if (HRESULT hr = dc->CreateCompatibleRenderTarget(&atlasSize, nullptr, &pixelFormat, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &bitmapRenderTarget); hr != S_OK) {
		LogError("CreateCompatibleRenderTarget() failed. HRESULT: %s", StrHr(hr));
		return false;
	}
	DEFER(bitmapRenderTarget->Release());
	
	bitmapRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);	
	bitmapRenderTarget->BeginDraw();
	bitmapRenderTarget->Clear();
	
	for (int i = 1; i < ICON_COUNT; i++) {
		if (!AddIconToAtlas(bitmapRenderTarget, i))
			dc->FillRectangle(SourceRect(i), UseColor(COLOR_WHITE));
	}
		
	if (HRESULT hr = bitmapRenderTarget->EndDraw(); hr != S_OK) {
		LogError("EndDraw() failed. HRESULT: %s", StrHr(hr));
		return false;
	}
	
	if (HRESULT hr = bitmapRenderTarget->GetBitmap(&iconAtlas); hr != S_OK) {
		LogError("GetBitmap() failed. HRESULT: %s", StrHr(hr));
		return false;
	}
	
	return true;
} 

void Icons::Shutdown() {
	if (iconAtlas) {
		iconAtlas->Release(); 
		iconAtlas = nullptr;
	}
}