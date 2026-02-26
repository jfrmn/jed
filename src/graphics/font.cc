 #include "font.hh"

#include "util/logging.hh"
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
		LogError("GetSystemFontCollection() failed. HRESULT: % ", FHr(hr));
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
			LogError("GetMatchingFonts() failed. HRESULT: %", FHr(hr));
			return false;
		}

		if (matchingFonts->GetFontCount() == 0) {
			LogError("no matching font in font family found");
			return false;
		}

		IDWriteFont* dwfont = nullptr;
		if (HRESULT hr = matchingFonts->GetFont(0, &dwfont); hr != S_OK) {
			LogError("GetFont() failed. HRESULT: %", FHr(hr));
			return false;
		}
		DEFER(dwfont->Release());;

		IDWriteFontFace* fontFace = nullptr;
		if (HRESULT hr = dwfont->CreateFontFace(&fontFace); hr != S_OK) {
			LogError("CreateFontFace() failed. HRESULT: %", FHr(hr));
			return false;
		}
		DEFER(fontFace->Release());;

		if (HRESULT hr = fontFace->QueryInterface(&fontFace1); hr != S_OK) {
			LogError("QueryInterface() for IDWriteFontFace1 failed. HRESULT: %", FHr(hr));
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void PrepareMeasureTextMemory(FontMeasureTextMemory* self, u64 reqSize) {
	
	if (self->capacity < reqSize) {
		
		const usize newMemSize = (sizeof(u32) * reqSize)
                               + (sizeof(s32) * reqSize)
		                       + (sizeof(u16) * reqSize);
		                       
		self->memory.reset(new s8[newMemSize]);
		
		self->utf32Text     = reinterpret_cast<u32*>(self->memory.get());
		self->glyphAdvances = reinterpret_cast<s32*>(self->utf32Text + reqSize);
		self->glyphIndicies = reinterpret_cast<u16*>(self->glyphAdvances + reqSize);
		self->capacity = reqSize;
	
	}
	
	const usize memorySize = (sizeof(u32) * self->capacity)
		                   + (sizeof(s32) * self->capacity)
		                   + (sizeof(u16) * self->capacity);
	memset(self->memory.get(), 0, memorySize);
}

float Font::MeasureText(std::string_view text, FontMeasureTextMemory* mem /*= nullptr*/) const {
	
	FontMeasureTextMemory backup {};
	if (!mem) mem = &backup;
	
	usize reqSize = 0u;
	ToUtf32(text, {}, &reqSize);
	
	PrepareMeasureTextMemory(mem, reqSize);
	
	ToUtf32(text, {mem->utf32Text, mem->capacity}, &mem->size);
	ASSERT(mem->size == reqSize);
	
	fontFace->GetGlyphIndices(mem->utf32Text,
	                          static_cast<UINT32>(mem->size),
	                          mem->glyphIndicies);
	fontFace->GetDesignGlyphAdvances(static_cast<UINT32>(mem->size),
	                                 mem->glyphIndicies,
	                                 mem->glyphAdvances);
	s32 totalAdvance = 0;
	for (u32 i = 0; i < mem->size; i++)
		totalAdvance += mem->glyphAdvances[i];

	return ConvertFromDesignUnits(totalAdvance);
}

Font::~Font() noexcept {
	if (fontFace)
		fontFace->Release();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// json mapping
//

#if 0
FROM_JSON_ENUM_DEFINITION_BEGIN(Font::Weight)
	FROM_JSON_ENUM_MEMBER(Weight_Light, "light")
	FROM_JSON_ENUM_MEMBER(Weight_Semi_Leight, "semi-leight")
	FROM_JSON_ENUM_MEMBER(Weight_Normal, "normal")
	FROM_JSON_ENUM_MEMBER(Weight_Semi_Bold, "semi-bold")
	FROM_JSON_ENUM_MEMBER(Weight_Bold, "bold")
FROM_JSON_ENUM_DEFINITION_END

FROM_JSON_ENUM_DEFINITION_BEGIN(Font::Style)
	FROM_JSON_ENUM_MEMBER(Style_Normal, "normal")
	FROM_JSON_ENUM_MEMBER(Style_Oblique, "oblique")
	FROM_JSON_ENUM_MEMBER(Style_Italic, "italic")
FROM_JSON_ENUM_DEFINITION_END

FROM_JSON_ENUM_DEFINITION_BEGIN(Font::Stretch)
	FROM_JSON_ENUM_MEMBER(Stretch_Normal, "normal")
	FROM_JSON_ENUM_MEMBER(Stretch_Expanded, "expanded")
	FROM_JSON_ENUM_MEMBER(Stretch_Condensed, "condensed")
FROM_JSON_ENUM_DEFINITION_END

template<class TEnum>
static bool FromJsonFontEnumOrNumber(cJSON* json, s32* result) {
	if ((json->type & 0xFF) == cJSON_Number) {
		return FromJson(json, result);
	} else if ((json->type & 0xFF) == cJSON_String) {
		return FromJson(json, reinterpret_cast<TEnum*>(result));
	} else {
		LogJsonError("expected value to be a <{}> or <{}> but was <{}>", JsonTypeToString(cJSON_Number), JsonTypeToString(cJSON_String), JsonTypeToString(json->type));
		return false;
	}
}

FROM_JSON_DEFINITION_BEGIN(Font::Description)
	FROM_JSON_PROPERTY_REQUIRED(name)
	FROM_JSON_PROPERTY_REQUIRED(size)
	FROM_JSON_PROPERTY_CUSTOM(weight, FromJsonFontEnumOrNumber<Font::Weight>)
	FROM_JSON_PROPERTY_CUSTOM(style, FromJsonFontEnumOrNumber<Font::Style>)
	FROM_JSON_PROPERTY_CUSTOM(stretch, FromJsonFontEnumOrNumber<Font::Stretch>)
	FROM_JSON_CHECK_UNRECOGNIZED
FROM_JSON_DEFINITION_END

bool FromJson(const cJSON* json, Font* font) {
	Font::Description description {};
	if (!FromJson(json, &description))
		return false;

	if (!font->Init(description)) {
		LogJsonError("font init failed");
		return false;
	}

	return true;
}
#endif