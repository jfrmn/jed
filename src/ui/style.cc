#include "style.hh"
#include "basic.hh"
#include "main-window.hh"

#include "util/file-util.hh"

#include "graphics/font.hh"
#include "graphics/factories.hh"

#include "json/json-mapping.hh"
#include "json/json-mapping-stl.h"

#include <cJSON/cJSON.h>

#include <wincodec.h>
#undef LoadBitmap

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Style style = {};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::string_view colorNames[] {
	"unknown",
	"glow",
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

static_assert(STATIC_ARRAY_SIZE(colorNames) == Style::Color_MAX);

static constexpr D2D1_COLOR_F defaultColors[] {
	{1.0f, 0.0f, 0.1f, 1.0f},  // unknown
	{0.2f, 0.3f, 0.6f, 1.0f},  // glow
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

static_assert(STATIC_ARRAY_SIZE(defaultColors) == Style::Color_MAX);

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

static_assert(STATIC_ARRAY_SIZE(iconFilenames) == Style::Icon_MAX);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static JSON_TO_ENUM_BEGIN(Style::BrushType)
	JSON_TO_ENUM_MEMBER("none", Style::BrushType_None)
	JSON_TO_ENUM_MEMBER("solid", Style::BrushType_SolidColor)
	JSON_TO_ENUM_MEMBER("linear", Style::BrushType_LinearGradient)
	JSON_TO_ENUM_MEMBER("radial", Style::BrushType_RadialGradient)
JSON_TO_ENUM_END

static JSON_TO_VALUE_BEGIN(D2D1_GRADIENT_STOP)
	JSON_TO_VALUE_PROPERTY_REQUIRED(position)
	JSON_TO_VALUE_PROPERTY_REQUIRED(color)
JSON_TO_VALUE_END

static JSON_TO_UNION_BEGIN(Style::BrushDescription, Style::BrushDescription::type)
	JSON_TO_UNION_PROPERTIES_FOR_TAG(Style::BrushType_SolidColor)
		JSON_TO_VALUE_PROPERTY_REQUIRED(color)
	JSON_TO_UNION_PROPERTIES_FOR_TAG(Style::BrushType_LinearGradient)
		JSON_TO_VALUE_PROPERTY_REQUIRED(stops)
		JSON_TO_VALUE_PROPERTY(repeatMirrored)
	JSON_TO_UNION_PROPERTIES_FOR_TAG(Style::BrushType_LinearGradient)
		ASSERT_NOT_IMPLEMENTED
	JSON_TO_VALUE_CHECK_UNRECOGNIZED
JSON_TO_UNION_END

// @TODO respect weight, stretch and so on
// also use JSON_TO_VALUE_CHECK_UNRECOGNIZED
static JSON_TO_VALUE_BEGIN(Font::Description)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(size)
JSON_TO_VALUE_END


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LoadColors(Style* self, const cJSON* json) {
	
	std::unordered_map<std::string_view, cJSON*> userColors {};
	
	const JsonTrace trace {nullptr, "colors"};
	if (const cJSON* jsonColor = cJSON_GetObjectItem(json, "colors")) {
		JsonObjectToMap(&trace, jsonColor, &userColors);
	}
		
	for (int i = 0; i < Style::Color_MAX; i++) {
				
		bool ok = false;
		if (auto node = userColors.extract(colorNames[i]); !node.empty()) {
			const JsonTrace traceColor {&trace, colorNames[i]};
			
			ok = JsonToValue(&traceColor, node.mapped(), &self->colors[i]);
		}

		if (!ok) self->colors[i] = defaultColors[i];
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

static bool LoadIcons(Style* self, const cJSON* json, ID2D1DeviceContext* deviceContext) {

	
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
	for (int i = 0; i < Style::Icon_MAX; i++) {
		
		LogDetail("loading icon: '%'", iconFilenames[i].data());
		
		fullIconPath.clear();
		fullIconPath.reserve(iconsDir.size() + iconFilenames[i].size());
		fullIconPath.append(iconsDir.begin(), iconsDir.end());
		fullIconPath.append(iconFilenames[i]);
		
		self->icons[i] = LoadBitmap(deviceContext, fullIconPath, false);
		
		// loading successfull - next icon
		if (!self->icons[i]) {
				
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
			self->icons[i] = dummyIcon;
			dummyIcon->AddRef();
		}
	}
		
	if (dummyIcon)
		dummyIcon->Release();
		
	return true;
}

static bool LoadAccentBrush(Style* self, const cJSON* json) {
	
	self->accentBrushDescription = Style::BrushDescription {
		.type = Style::BrushType_LinearGradient,
		.repeatMirrored = true,
		.stops = {
			D2D1_GRADIENT_STOP {
				.position = 0.0f,
				//.color = {0.57f, 0.43f, 0.85f, 1.00f}}, // 145, 109, 216
				.color = {0.39f, 0.13f, 0.90f, 1.00f}}, // 100, 34, 230
			D2D1_GRADIENT_STOP {
				.position = 1.0f,
				.color = {0.00f, 0.80f, 0.80f, 1.00f}}}};
	
	if (const cJSON* jsonAccent = cJSON_GetObjectItem(json, "accent-brush")) {
		const JsonTrace trace {nullptr, "accent-brush"};
		
		// Use an extra copy here!
		// If JsonToValue fails somewhere deep in the callstack 
		// it has already overwritten half of the properties
		Style::BrushDescription brushDescriptionFromJson {};
		if (JsonToValue(&trace, jsonAccent, &brushDescriptionFromJson))
			self->accentBrushDescription = std::move(brushDescriptionFromJson);
	}
	
	if (self->accentBrushDescription.type == Style::BrushType_SolidColor) {
		ID2D1SolidColorBrush* solidBrush = nullptr;
		const HRESULT hr = mainWindow.deviceContext->CreateSolidColorBrush(self->accentBrushDescription.color, &solidBrush);
		
		if (hr != S_OK) {
			LogError("CreateSolidColorBrush() failed. HRESULT: %", FHr(hr));
			return false;
		}
		
		self->accentBrush = solidBrush;
	
	} else if (self->accentBrushDescription.type == Style::BrushType_LinearGradient) {
		
		ID2D1GradientStopCollection* gradientStopCollection = nullptr;
		const HRESULT hr = mainWindow.deviceContext->CreateGradientStopCollection(
			self->accentBrushDescription.stops.data(),
			static_cast<UINT32>(self->accentBrushDescription.stops.size()),
			D2D1_GAMMA_2_2,
			(self->accentBrushDescription.repeatMirrored
				? D2D1_EXTEND_MODE_MIRROR
				: D2D1_EXTEND_MODE_WRAP),
			&gradientStopCollection);
	
		if (hr != S_OK) {
			LogError("CreateGradientStopCollection() failed. HRESULT: %", FHr(hr));
			return false;
		}
		
		DEFER(gradientStopCollection->Release());
		
		ID2D1LinearGradientBrush* gradientBrush = nullptr;
		if (HRESULT hr = mainWindow.deviceContext->CreateLinearGradientBrush(
				D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES {},
				gradientStopCollection,
				&gradientBrush); hr != S_OK) {
			LogError("CreateLinearGradientBrush() failed. HRESULT: %", FHr(hr));
			return false;
		}

		self->accentBrush = gradientBrush;
		return true;
	
	} else if (self->accentBrushDescription.type == Style::BrushType_RadialGradient) {
		ASSERT_NOT_IMPLEMENTED
		return false;
	
	} else {
		ASSERT_UNREACHABLE
		return false;
	}

	return true;
}

bool Style::Init(const cJSON* json, ID2D1DeviceContext* deviceContext) {
	if (!LoadColors(this, json)) return false;
	if (!LoadFont(&this->fontEditor, json, "font-editor", Font::Description {"Consolas", 14})) return false;
	if (!LoadFont(&this->fontUi,     json, "font-ui",     Font::Description {"Microsoft Sans Serif", 14})) return false;
	if (!LoadIcons(this, json, deviceContext)) return false;
	if (!LoadAccentBrush(this, json)) return false;
	
	if (HRESULT hr = deviceContext->CreateSolidColorBrush(D2D_COLOR_F {}, &brush); hr != S_OK) {
		LogError("CreateSolidColorBrush() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ID2D1Brush* Style::GetAccentBrush(const D2D1_RECT_F* area /*= nullptr*/, f32 animationValue /*= 0.0f*/) {
	if (accentBrushDescription.type == BrushType_SolidColor) {
		return accentBrush;
	
	} else if (accentBrushDescription.type == BrushType_LinearGradient) {
		
		if (area) {
						
			const D2D_VECTOR_2F vector {(area->right - area->left) * 2, (area->bottom - area->top) * 2};
			const f32 vectorLength = std::sqrt(
				(vector.x * vector.x) +
				(vector.y * vector.y));
			
			const f32 offsetFactor = std::cos(animationValue + F32_PI) + 1.0f;
			
			auto linearGradientBrush = static_cast<ID2D1LinearGradientBrush*>(accentBrush);
			linearGradientBrush->SetStartPoint(D2D1_POINT_2F {
				.x = area->left + (vector.x * offsetFactor),
				.y = area->top  + (vector.y * offsetFactor)});
			linearGradientBrush->SetEndPoint(D2D1_POINT_2F {
				.x = area->right   + (vector.x * offsetFactor),
				.y = area->bottom  + (vector.y * offsetFactor)});
		}
		return accentBrush;
		
	} else if (accentBrushDescription.type == BrushType_RadialGradient) {
		ASSERT_NOT_IMPLEMENTED;
		return nullptr;
	
	} else {
		ASSERT_UNREACHABLE;
		return nullptr;
	}
}
	
ID2D1SolidColorBrush* Style::GetBrushGlow() {
	brush->SetColor(colors[Color_Glow]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushSelection(bool active /*= true*/) {
	brush->SetColor(colors[active
		? Color_Selection
		: Color_SelectionInactive]);
	
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushUiSearchResult() {
	brush->SetColor(colors[Color_UiSearchResult]);	
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushEditorText() {
	brush->SetColor(colors[Color_EditorText]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushEditorBackground() {
	brush->SetColor(colors[Color_EditorBackground]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushEditorMultiCaretEdit() {
	brush->SetColor(colors[Color_EditorMultiCaretEdit]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushUiText(bool active /*= true*/) {
	brush->SetColor(colors[active
		? Color_UiText
		: Color_UiTextInactive]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushUiBackground(bool active /*= true*/) {
	brush->SetColor(colors[active
		? Color_UiBackground
		: Color_UiBackgroundInactive]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushUiBackgroundInvalid() {
	brush->SetColor(colors[Color_UiBackgroundInvalid]);
	return brush;
}

ID2D1SolidColorBrush* Style::GetBrushHover(bool pressed /*= false*/) {
	brush->SetColor(colors[pressed
		? Color_Pressed
		: Color_Hover]);
	return brush;
}
	
ID2D1SolidColorBrush* Style::GetBrushToggled() {
	brush->SetColor(colors[Color_Toggled]);
	return brush;
}
	
Style::~Style() noexcept {
	for (int i = 0; i < Icon_MAX; i++)
		icons[i]->Release();
	
	if (brush)
		brush->Release();
		
	if (accentBrush)
		accentBrush->Release();
}
