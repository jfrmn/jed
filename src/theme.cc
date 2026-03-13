#include "theme.hh"
#include "basic.hh"
#include "main-window.hh"

#include "util/file-util.hh"

#include "graphics/font.hh"
#include "graphics/factories.hh"
#include "graphics/effects.hh"

#include "json/json-mapping.hh"
#include "json/json-mapping-stl.h"

#include <cJSON/cJSON.h>

#include <wincodec.h>
#undef LoadBitmap

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Theme theme = {};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::string_view colorNames[] {
	"unknown",
	"drop-shadow",
	"active-panel-frame",
	"selection",
	"selection-inactive",
	"ui-hover",
	"ui-pressed",
	"ui-toggled",
	"editor-text",
	"editor-background",
	"editor-multi-caret-edit",
	"ui-text",
	"ui-text-inactive",
	"ui-search-result",
	"ui-background",
	"ui-background-inactive",
	"ui-background-invalid"
};

static_assert(STATIC_ARRAY_SIZE(colorNames) == Theme::NUM_COLORS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr D2D1_COLOR_F defaultColors[] {
	{1.0f, 0.0f, 0.1f, 1.0f},  // unknown
	{0.2f, 0.3f, 0.6f, 1.0f},  // drop-shadow
	{0.2f, 0.3f, 0.6f, 1.0f},  // active-panel-frame
	{0.0f, 1.0f, 1.0f, 0.3f},  // selection
	{1.0f, 1.0f, 1.0f, 0.3f},  // selection-inactive
	{1.0f, 1.0f, 1.0f, 0.5f},  // hover
	{0.8f, 0.8f, 0.8f, 0.5f},  // pressed
	{0.8f, 0.8f, 0.8f, 0.5f},  // toggled
	{1.0f, 1.0f, 1.0f, 1.0f},  // editor-text
	{0.1f, 0.1f, 0.1f, 1.0f},  // editor-background
	{1.0f, 0.0f, 1.0f, 1.0f},  // editor-multi-caret-edit
	{1.0f, 1.0f, 1.0f, 1.0f},  // ui-text
	{0.6f, 0.6f, 0.6f, 1.0f},  // ui-text-inactive
	{1.0f, 1.0f, 0.0f, 0.3f},  // ui-search-result
	{0.3f, 0.3f, 0.3f, 1.0f},  // ui-background
	{0.2f, 0.2f, 0.2f, 1.0f},  // ui-background-inactive
	{0.4f, 0.0f, 0.0f, 1.0f}}; // ui-background-invalid 

static_assert(STATIC_ARRAY_SIZE(defaultColors) == Theme::NUM_COLORS);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::wstring_view iconFilenames[] {
	L"unknown.png",
	L"waiting.png",
	L"error.png",
	L"noitems.png",
	L"tabs-modified-hovered.png",
	L"tabs-modified.png",
	L"tabs-hovered.png",
	L"editor-diagnostics-error.png",
	L"editor-diagnostics-warning.png",
	L"editor-diagnostics-info.png",
	L"editor-diagnostics-hint.png",
	L"editor-autocomplete-text.png",
	L"editor-autocomplete-method.png",
	L"editor-autocomplete-function.png",
	L"editor-autocomplete-constructor.png",
	L"editor-autocomplete-field.png",
	L"editor-autocomplete-variable.png",
	L"editor-autocomplete-class.png",
	L"editor-autocomplete-interface.png",
	L"editor-autocomplete-module.png",
	L"editor-autocomplete-property.png",
	L"editor-autocomplete-unit.png",
	L"editor-autocomplete-value.png",
	L"editor-autocomplete-enum.png",
	L"editor-autocomplete-keyword.png",
	L"editor-autocomplete-snippet.png",
	L"editor-autocomplete-color.png",
	L"editor-autocomplete-file.png",
	L"editor-autocomplete-reference.png",
	L"editor-autocomplete-folder.png",
	L"editor-autocomplete-enum-member.png",
	L"editor-autocomplete-constant.png",
	L"editor-autocomplete-struct.png",
	L"editor-autocomplete-event.png",
	L"editor-autocomplete-operator.png",
	L"editor-autocomplete-type-parameter.png",
	L"editor-search-resultsclosed.png",
	L"editor-search-resultsopened.png",
	L"explorer-folder-open.png",
	L"explorer-folder-closed.png",
	L"explorer-file.png",
	L"lsp-standby.png",
	L"lsp-initializing.png",
	L"lsp-running.png",
	L"lsp-shuttingDown.png",
	L"lsp-exited.png",
	L"lsp-crashed.png"
};

static_assert(STATIC_ARRAY_SIZE(iconFilenames) == Theme::NUM_ICONS);

// @TODO respect weight, stretch and so on
// also use JSON_TO_VALUE_CHECK_UNRECOGNIZED
static JSON_TO_VALUE_BEGIN(Font::Description)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(size)
JSON_TO_VALUE_END

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LoadColors(Theme* self, const cJSON* json) {
	
	std::unordered_map<std::string_view, cJSON*> userColors {};
	
	const JsonTrace trace {nullptr, "colors"};
	if (const cJSON* jsonColor = cJSON_GetObjectItem(json, "colors")) {
		JsonObjectToMap(&trace, jsonColor, &userColors);
	}
		
	for (int i = 0; i < Theme::NUM_COLORS; i++) {
				
		bool ok = false;
		if (auto node = userColors.extract(colorNames[i]); !node.empty()) {
			const JsonTrace traceColor {&trace, colorNames[i]};
			
			ok = JsonToValue(&traceColor, node.mapped(), &self->colorArray[i]);
		}

		if (!ok) self->colorArray[i] = defaultColors[i];
	}
	
	for (auto it = userColors.begin(); it != userColors.end(); ++it) {
		const JsonTrace traceColor {&trace, it->first};
		JsonLogWarning(&traceColor, "unknown color");
	}
	
	return true;
}

static bool LoadFont(Font* font, const cJSON* json, std::string_view propertyInJson, const Font::Description& fallback) {
	
	bool ok = false;
	if (const cJSON* jsonFont = cJSON_GetObjectItem(json, propertyInJson.data())) {
		const JsonTrace trace {nullptr, propertyInJson};
			
		Font::Description fontDesc {};
		if (JsonToValue(&trace, jsonFont, &fontDesc)) {
			ok = font->Init(fontDesc);
		}
		
	}
		
	if (!ok) {
		if (!font->Init(fallback)) {
			LogError("failed to load font for UI");
			return false;
		}
	}
	
	return true;
}

static ID2D1Bitmap* LoadBitmap(ID2D1DeviceContext* deviceContext, std::wstring_view filePath, bool alphaOnly /*= false*/) {

	if (GetFileAttributesW(filePath.data()) == INVALID_FILE_ATTRIBUTES) {
		LogError("file does not exists : '%'", filePath.data());
		return nullptr;
	}

	IWICBitmapDecoder* decoder = nullptr;
	if (HRESULT hr = wicFactory->CreateDecoderFromFilename(filePath.data(),	NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder); hr != S_OK) {
		LogError("CreateDecoderFromFilename() failed");
		return nullptr;
	}
	DEFER(decoder->Release());

	IWICBitmapFrameDecode* frameDecode = nullptr;
	if (HRESULT hr = decoder->GetFrame(0, &frameDecode); hr != S_OK) {
		LogError("failed to decode frame. HRESULT: %", FHr(hr));
		return nullptr;
	}
	DEFER(frameDecode->Release());
	
	IWICFormatConverter* converter = nullptr;
	if (HRESULT hr = wicFactory->CreateFormatConverter(&converter); hr != S_OK) {
		LogError("CreateFormatConverter() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	DEFER(converter->Release());
	
	if (HRESULT hr = converter->Initialize(
			frameDecode,
			alphaOnly
				? GUID_WICPixelFormat8bppAlpha
				: GUID_WICPixelFormat32bppPRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeMedianCut); hr != S_OK) {
		LogError("failed to initialize converter. HRESULT: %", FHr(hr));
		return nullptr;
	}

	IWICBitmap* wicBitmap = nullptr;
	if (HRESULT hr = wicFactory->CreateBitmapFromSource(converter, WICBitmapCreateCacheOption::WICBitmapCacheOnDemand, &wicBitmap); hr != S_OK) {
		LogError("CreateBitmapFromSource() failed. HRESULT: %", FHr(hr));
		return nullptr;
	}
	DEFER(wicBitmap->Release());
	
	ID2D1Bitmap* bitmap = nullptr;
	if (HRESULT hr = deviceContext->CreateBitmapFromWicBitmap(wicBitmap, &bitmap); hr != S_OK) {
		LogError("failed to create ID2D1Bitmap from IWICBitmap. HRESULT: %", FHr(hr));
		return nullptr;
	}

	return bitmap;
}

static bool LoadIcons(Theme* self, const cJSON* json, ID2D1DeviceContext* deviceContext) {

	
#ifdef _DEBUG
	std::string_view iconsDir = ".\\assets\\";
#else
	const std::string defaultDir = GetProcessDirectory() + "assets\\";
	std::string_view iconsDir = defaultDir;
#endif

	if (const cJSON* jsonIconsDir = cJSON_GetObjectItem(json, "icons-dir")) {
		const JsonTrace trace {nullptr, "icons-dir"};
		
		if (JsonCheckType(&trace, jsonIconsDir, cJSON_String)) {
			iconsDir = jsonIconsDir->valuestring;
		}
	}
	
	LogInfo("loading icons from '%'", iconsDir);
	
	std::wstring fullIconPath {};
	ID2D1Bitmap* dummyIcon = nullptr;
	for (int i = 0; i < Theme::NUM_ICONS; i++) {
		
		LogDetail("loading icon: '%'", iconFilenames[i].data());
		
		fullIconPath.clear();
		fullIconPath.reserve(iconsDir.size() + iconFilenames[i].size());
		fullIconPath.append(iconsDir.begin(), iconsDir.end());
		fullIconPath.append(iconFilenames[i]);
		
		self->iconArray[i] = LoadBitmap(deviceContext, fullIconPath, false);
		
		// loading successfull - next icon
		if (!self->iconArray[i]) {
				
			// dummy icon not load yet
			if (!dummyIcon) {
				ID2D1BitmapRenderTarget* bitmapRenderTarget = nullptr;
				if (HRESULT hr = mainWindow.deviceContext->CreateCompatibleRenderTarget({32.0f, 32.0f}, &bitmapRenderTarget); hr != S_OK) {
					LogError("CreateCompatibleRenderTarget() failed. HRESULT: %", FHr(hr));
					return false;
				}
				
				DEFER(bitmapRenderTarget->Release());
						
				bitmapRenderTarget->BeginDraw();
				bitmapRenderTarget->Clear(D2D1_COLOR_F {1.0f, 0.0f, 1.0f, 1.0f});
					
				if (HRESULT hr = bitmapRenderTarget->EndDraw(); hr != S_OK) {
					LogError("EndDraw() failed. HRESULT: %", FHr(hr));
					return false;
				}
					
				if (HRESULT hr = bitmapRenderTarget->GetBitmap(&dummyIcon); hr != S_OK) {
					LogError("GetBitmap() failed. HRESULT: %", FHr(hr));
					return false;
				}
			}
					
			ASSERT(dummyIcon)
			self->iconArray[i] = dummyIcon;
			dummyIcon->AddRef();
		}
	}
		
	if (dummyIcon)
		dummyIcon->Release();
		
	return true;
}

bool Theme::Init(const cJSON* json, ID2D1DeviceContext* deviceContext) {
	if (!LoadColors(this, json)) return false;
	if (!LoadFont(&this->fontEditor, json, "font-editor", Font::Description {"Consolas", 14})) return false;
	if (!LoadFont(&this->fontUi,     json, "font-ui",     Font::Description {"Microsoft Sans Serif", 14})) return false;
	if (!LoadIcons(this, json, deviceContext)) return false;
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1SolidColorBrush* Theme::GetBrushGlow() {
	brush->SetColor(colors.dropShadow);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushSelection(bool active /*= true*/) {
	brush->SetColor(active
		? colors.selection
		: colors.selectionInactive);
	
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushUiSearchResult() {
	brush->SetColor(colors.uiSearchResult);	
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushEditorText() {
	brush->SetColor(colors.editorText);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushEditorBackground() {
	brush->SetColor(colors.editorBackground);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushEditorMultiCaretEdit() {
	brush->SetColor(colors.editorMultiCaretEdit);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushUiText(bool active /*= true*/) {
	brush->SetColor(active
		? colors.uiText
		: colors.uiTextInactive);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushUiBackground(bool active /*= true*/) {
	brush->SetColor(active
		? colors.uiBackground
		: colors.uiBackgroundInactive);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushUiBackgroundInvalid() {
	brush->SetColor(colors.uiBackgroundInvalid);
	return brush;
}

ID2D1SolidColorBrush* Theme::GetBrushHover(bool pressed /*= false*/) {
	brush->SetColor(pressed
		? colors.pressed
		: colors.hover);
	return brush;
}
	
ID2D1SolidColorBrush* Theme::GetBrushToggled() {
	brush->SetColor(colors.toggled);
	return brush;
}
	
Theme::~Theme() noexcept {
	for (int i = 0; i < NUM_ICONS; i++)
		iconArray[i]->Release();
}
