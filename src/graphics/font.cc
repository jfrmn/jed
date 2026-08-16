 #include "font.hh"

#include "logging.hh"
#include "util/string-util.hh"

#include "graphics/factories.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <dwrite_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Font::Init(const Description& desc) {
	
	//
	// load system font collection
	//
	IDWriteFontCollection* fontCollection = nullptr;
	if (auto hr = dwFactory->GetSystemFontCollection(&fontCollection); hr != S_OK) {
		LogError("GetSystemFontCollection() failed. HRESULT: % ", StrHr(hr));
		return false;
	}
	DEFER(fontCollection->Release());;
	
	//
	// find font familiy
	//
	IDWriteFontFamily* fontFamily = nullptr;
	{
		wchar targetFontNameBuf[256];
		usize targetFontNameLen = 0u;
		if (!ToUtf16(desc.name, targetFontNameBuf, &targetFontNameLen)) {
			LogError("failed to convert font name to utf16");
			return false;
		}

		const std::wstring_view targetFontName {targetFontNameBuf, targetFontNameLen};

		for (u32 i = 0; i < fontCollection->GetFontFamilyCount(); i++) {

			IDWriteFontFamily* currentFontFamily = nullptr;
			fontCollection->GetFontFamily(i, &currentFontFamily);
			if (!currentFontFamily) continue;
			DEFER(currentFontFamily->Release());;

			IDWriteLocalizedStrings* fontFamilyNames = nullptr;
			currentFontFamily->GetFamilyNames(&fontFamilyNames);
			if (!fontFamilyNames) continue;
			DEFER(fontFamilyNames->Release());;

			for (u32 i = 0; i < fontFamilyNames->GetCount(); i++) {

				u32 currentNameLen = 0;
				fontFamilyNames->GetStringLength(i, &currentNameLen);
				
				wchar currentNameBuf[256] {};
				fontFamilyNames->GetString(i, currentNameBuf, 255); // only 255 because of the nullterminator

				const std::wstring_view currentName {currentNameBuf, currentNameLen};
				if (currentName == targetFontName)
				{
					fontFamily = currentFontFamily;
					fontFamily->AddRef();
					goto found_font_family;
				}
			}
		}

		LogError("font family not found");
		return false;
	}
	
	found_font_family:
	DEFER(fontFamily->Release());;

	//
	// load font face
	//

	IDWriteFontFace1* fontFace1 = nullptr;
	{
		IDWriteFontList* matchingFonts = nullptr;
		if (HRESULT hr = fontFamily->GetMatchingFonts(
				static_cast<DWRITE_FONT_WEIGHT>(desc.weight),
				static_cast<DWRITE_FONT_STRETCH>(desc.stretch),
				static_cast<DWRITE_FONT_STYLE>(desc.style),
				&matchingFonts); hr != S_OK) {
			LogError("GetMatchingFonts() failed. HRESULT: %", StrHr(hr));
			return false;
		}

		if (matchingFonts->GetFontCount() == 0) {
			LogError("no matching font in font family found");
			return false;
		}

		IDWriteFont* dwfont = nullptr;
		if (HRESULT hr = matchingFonts->GetFont(0, &dwfont); hr != S_OK) {
			LogError("GetFont() failed. HRESULT: %", StrHr(hr));
			return false;
		}
		DEFER(dwfont->Release());;

		IDWriteFontFace* fontFace = nullptr;
		if (HRESULT hr = dwfont->CreateFontFace(&fontFace); hr != S_OK) {
			LogError("CreateFontFace() failed. HRESULT: %", StrHr(hr));
			return false;
		}
		DEFER(fontFace->Release());;

		if (HRESULT hr = fontFace->QueryInterface(&fontFace1); hr != S_OK) {
			LogError("QueryInterface() for IDWriteFontFace1 failed. HRESULT: %", StrHr(hr));
			return false;
		}
	}

	//
	// prepare result
	//
	{
		const u32 space = U' ';
		u16 glyphIndexSpace = 0;
		fontFace1->GetGlyphIndices(&space, 1, &glyphIndexSpace);
		
		DWRITE_FONT_METRICS metrics {};
		fontFace1->GetMetrics(&metrics);

		this->fontFace = fontFace1;
		this->size = desc.size;
		this->designUnitsPerEm = metrics.designUnitsPerEm;
		this->baselineOffset = ConvertFromDesignUnits(metrics.ascent);
		this->underlineOffset = this->baselineOffset - ConvertFromDesignUnits(metrics.underlinePosition);
		this->strikethroughOffset = this->baselineOffset - ConvertFromDesignUnits(metrics.strikethroughPosition);
		this->lineHeight = ConvertFromDesignUnits(metrics.ascent + metrics.descent + metrics.lineGap);
		this->glyphIndexSpace = glyphIndexSpace;
	}

	return true;
}

void Font::ChangeFontSize(f32 newFontSize) {
	ASSERT_NOT_IMPLEMENTED;
}

f32 Font::GetSpaceAdvance() const {

	s32 designGlyphAdvance = 0;
	fontFace->GetDesignGlyphAdvances(1, &glyphIndexSpace, &designGlyphAdvance, FALSE);

	return ConvertFromDesignUnits(designGlyphAdvance);
}

f32 Font::ConvertFromDesignUnits(s32 designUnits) const {
	return designUnits * size / designUnitsPerEm;
}

Font::~Font() noexcept {
	if (fontFace)
		fontFace->Release();
}
