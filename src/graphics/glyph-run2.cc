#include "glyph-run2.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dwrite.h>

struct TextAnalysisrSource : public IDWriteTextAnalysisSource  {

	virtual HRESULT GetLocaleName(UINT32 textPosition, UINT32* textLength, WCHAR const** localeName) noexcept override {
		*localeName = L"en-US"; // @TODO fetch actual locale
		return S_OK;
	}
	
	virtual HRESULT GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubst) noexcept override {
		*numberSubst = nullptr;
		return S_OK;
	}
	
	virtual DWRITE_READING_DIRECTION GetParagraphReadingDirection() noexcept override {
		return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
	}
	
	virtual HRESULT GetTextAtPosition(UINT32 textPosition, WCHAR const** textString, UINT32* textLength) noexcept override {
		
	}
	
};

bool Shape(std::string_view text, const Font& font) {

}
