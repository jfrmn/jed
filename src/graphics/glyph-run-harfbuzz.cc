#include "glyph-run-harfbuzz.hh"
#include "font.hh"
#include "util/logging.hh"

#include <harfbuzz/src/hb.h>
#include <harfbuzz/src/hb-directwrite.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dwrite.h>
#include <d2d1.h>

static hb_buffer_t* hbStaticBuffer = nullptr;
static hb_language_t hbLanguage = nullptr;

static constexpr u64 STATIC_BLOB_BUFFER_SIZE = 1024;
static hb_blob_t* hbStaticBlob = nullptr;

bool GlyphRun_Harfbuzz::InitHarfbuzz() {
	hbStaticBuffer = hb_buffer_create();	
	if (!hb_buffer_allocation_successful(hbStaticBuffer)) {
		LogError("failed to allocate harfbuzz buffer");
		return false;
	}
	
	hbLanguage = hb_language_get_default();
	
	char* initialBlobData = new char[STATIC_BLOB_BUFFER_SIZE] {0};
	hbStaticBlob = hb_blob_create_or_fail(
		initialBlobData,
		STATIC_BLOB_BUFFER_SIZE,
		HB_MEMORY_MODE_WRITABLE,
		initialBlobData,
		[] (void* userdata) { delete[] static_cast<char*>(userdata); });
	
	if (!hbStaticBlob) {
		LogError("failed to create harfbuzz blob");
		return false;
	}
	
	return true;
}

static void PrepareGlyphRun(GlyphRun_Harfbuzz* self, u64 reqSize) {
	
	auto CalcMemSize = [](u64 size) {
		return (sizeof(*GlyphRun_Harfbuzz::advances) * size)
 		     + (sizeof(*GlyphRun_Harfbuzz::mapping)  * size)
 		     + (sizeof(*GlyphRun_Harfbuzz::indicies) * size);
	};
	
	if (self->capacity < reqSize) {

		const u64 newMemSize = CalcMemSize(reqSize);
		self->memory.reset(new s8[newMemSize]);

		self->advances = reinterpret_cast<f32*>(self->memory.get());
		self->mapping  = reinterpret_cast<u32*>(self->advances + reqSize);
		self->indicies = reinterpret_cast<u16*>(self->mapping  + reqSize);
		self->capacity = reqSize;
	}
	
	const u64 memSize = CalcMemSize(self->capacity);
	memset(self->memory.get(), 0, memSize);
	self->size = 0u;
}


bool GlyphRun_Harfbuzz::Shape(std::string_view text, const Font& font) {
	
	hb_buffer_clear_contents(hbStaticBuffer);
	
	u32 u32Len = static_cast<u32>(text.length());
	hb_buffer_add_utf8(hbStaticBuffer, text.data(), u32Len, 0, u32Len);
	
	hb_buffer_set_direction(hbStaticBuffer, HB_DIRECTION_LTR);
	hb_buffer_set_language(hbStaticBuffer, hbLanguage);
	hb_buffer_guess_segment_properties(hbStaticBuffer);

	hb_font_t* hbFont;
	// @TODO make member of font
	hbFont = hb_directwrite_font_create(font.fontFace);

	u32 memSize = 0u;
	char* mem = hb_blob_get_data_writable(hbStaticBlob, &memSize);
	memset(mem, 0, memSize);
	
	const char* shaper[] = { "ot" };
	hb_shape_full(hbFont, hbStaticBuffer, nullptr, 0, shaper);

	u32 glyphCount = 0u; u32 glyphCount2 = 0u;
	hb_glyph_info_t* hbGlyphInfo = hb_buffer_get_glyph_infos(hbStaticBuffer, &glyphCount);
	hb_glyph_position_t* hbGlyphPos = hb_buffer_get_glyph_positions(hbStaticBuffer, &glyphCount2);
	ASSERT(glyphCount == glyphCount2);
	
	PrepareGlyphRun(this, glyphCount);
		
	for (u32 i = 0; i < glyphCount; i++) {
    	indicies[i] = hbGlyphInfo[i].codepoint;
		mapping[i]  = hbGlyphInfo[i].cluster;
		advances[i] = font.ConvertFromDesignUnits(hbGlyphPos[i].x_advance);
	}
	size = glyphCount;
	
	hb_font_destroy(hbFont);
	return true;
}

void GlyphRun_Harfbuzz::Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const {
	if (size == 0u) return;
	
	const DWRITE_GLYPH_RUN glyphRun {
		.fontFace      = font.fontFace,
		.fontEmSize    = font.size,
		.glyphCount    = static_cast<u32>(size),
		.glyphIndices  = indicies,
		.glyphAdvances = advances,
		.glyphOffsets  = nullptr,
		.isSideways    = FALSE,
		.bidiLevel     = 0};

	renderTarget->DrawGlyphRun(
		D2D_POINT_2F {
			.x = x,
			.y = y + font.baselineOffset},
		&glyphRun,
		brush);
}
