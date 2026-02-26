#include "factories.hh"
#include "basic.hh"
#include "util/logging.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d2d1_1.h>
#include <d2d1effects_2.h>
#include <dwrite.h>
#include <wincodec.h>
#undef LoadBitmap

ID2D1Factory* d2dFactory;
IDWriteFactory* dwFactory;
IWICImagingFactory* wicFactory;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool InitFactories() {

	ASSERT(!d2dFactory);
	ASSERT(!dwFactory);
	ASSERT(!dwFactory);

	D2D1_FACTORY_OPTIONS options {};
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
	
	if (auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2dFactory); hr != S_OK) {
		LogError("D2D1CreateFactory() failed. HRESULT: %", FHr(hr));
		return false;
	}

	if (auto hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwFactory); hr != S_OK) {
		LogError("DWriteCreateFactory() failed. HRESULT: %", FHr(hr));
		return false;
	}

	if (auto hr = CoInitialize(NULL); hr != S_OK) {
		LogError("CoInitialize() failed. HRESULT: %", FHr(hr));
		return false;
	}

	if (auto hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory)); hr != S_OK) {
		LogError("Failed to create wicFactory! HRESULT: %", FHr(hr));
		return false;
	}

	return true;
}

void ShutdownFactories() {
	if (wicFactory) {
		wicFactory->Release();
		wicFactory = nullptr;
	}

	CoUninitialize();

	if (dwFactory) {
		dwFactory->Release();
		dwFactory = nullptr;
	}

	if (d2dFactory) {
		d2dFactory->Release();
		d2dFactory = nullptr;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IWICStream* MakeWicStream() {
	
	IWICStream* stream = nullptr;
	if (HRESULT hr = wicFactory->CreateStream(&stream); hr != S_OK) {
		LogError("CreateStream() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	return stream;
}

IWICBitmapDecoder* MakeDecoder(IWICStream* stream /*= nullptr*/) {

	IWICBitmapDecoder* decoder = nullptr;
	
	if (stream) {
		if (HRESULT hr = wicFactory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder); hr != S_OK) {
			LogError("CreateDecoderFromStream() failed. HRESULT: %", FHr(hr));
			return nullptr;
		}
	
	} else {
		if (HRESULT hr = wicFactory->CreateDecoder(GUID_ContainerFormatPng, nullptr, &decoder); hr != S_OK) {
			LogError("CreateDecoder() failed. HRESULT: %", FHr(hr));
			return nullptr;
		}
	}
	
	return decoder;
}


IWICFormatConverter* MakeFormatConverter() {

	IWICFormatConverter* converter = nullptr;
	if (HRESULT hr = wicFactory->CreateFormatConverter(&converter); hr != S_OK) {
		LogError("CreateFormatConverter() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	return converter;
}

ID2D1Bitmap* LoadBitmap(IWICStream* wicStream, IWICBitmapDecoder* decoder,  IWICFormatConverter* formatConverter, ID2D1RenderTarget* renderTarget, bool alphaOnly /*= false*/) {
	
	if (HRESULT hr = decoder->Initialize(wicStream, WICDecodeMetadataCacheOnDemand); hr != S_OK) {
		LogError("decoder->Initialize() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	
	IWICBitmapFrameDecode* frameDecode = nullptr;
	if (HRESULT hr = decoder->GetFrame(0, &frameDecode); hr != S_OK) {
		LogError("decoder->GetFrame() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}

	if (HRESULT hr = formatConverter->Initialize(
			frameDecode,
			alphaOnly ? GUID_WICPixelFormat8bppAlpha : GUID_WICPixelFormat32bppPRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut); hr != S_OK) {
		LogError("formatConverter->Initialize() failed. HRESULT: %", FHr(hr));
		frameDecode->Release();
		return nullptr;
	}
	
	IWICBitmap* wicBitmap = nullptr;
	if (HRESULT hr = wicFactory->CreateBitmapFromSource(formatConverter, WICBitmapCreateCacheOption::WICBitmapCacheOnDemand, &wicBitmap); hr != S_OK) {
		LogError("CreateBitmapFromSource() failed. HRESULT: %", FHr(hr));
		frameDecode->Release();
		return nullptr;
	}
	
	ID2D1Bitmap* bitmap = nullptr;
	if (HRESULT hr = renderTarget->CreateBitmapFromWicBitmap(wicBitmap, &bitmap); hr != S_OK) {
		LogError("CreateBitmapFromWicBitmap() failed. HRESULT: %", FHr(hr));
		frameDecode->Release();
		wicBitmap->Release();
		return nullptr;
	}
	
	frameDecode->Release();
	wicBitmap->Release();
	return bitmap;
}
