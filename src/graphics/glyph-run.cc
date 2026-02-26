#include "glyph-run.hh"
#include "font.hh"
#include "util/logging.hh"
#include "util/string-util.hh"

#define WIN32_LEAN_AND_MEAN
#include <d2d1.h>
#include <dwrite_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void PrepareShapingMemory(GlyphRunShapingMemory* self, u64 reqSize) {
	if (self->capacity < reqSize) {
		self->memory = std::make_unique<u32[]>(reqSize * 2);
		self->capacity = reqSize;
		self->utf32Text = static_cast<u32*>(self->memory.get());
		self->designGlyphAdvances = reinterpret_cast<s32*>(self->memory.get() + reqSize);
	}
	
	memset(self->memory.get(), 0, (self->capacity * sizeof(s32) * 2));
	self->size = 0u;
}

static void PrepareGlyphRun(GlyphRunBase* self, u64 reqSize)
{
	if (self->capacity < reqSize) {

		const usize newMemSize = (sizeof(f32) * reqSize) + (sizeof(u16) * reqSize);
		self->memory.reset(new s8[newMemSize]);

		self->advances = reinterpret_cast<f32*>(self->memory.get());
		self->indicies = reinterpret_cast<u16*>(self->advances + reqSize);
		self->capacity = reqSize;
	
	}
	
	const usize memSize = (sizeof(f32) * self->capacity) + (sizeof(u16) * self->capacity);
	memset(self->memory.get(), 0, memSize);
	self->size = 0u;
}

template<class TGlyphRun>
static bool ShapeInternal(TGlyphRun* run, std::string_view text, const Font& font, GlyphRunShapingMemory* mem) {

	 constexpr bool IS_MULTILINE = std::is_same<TGlyphRun, GlyphRunMultiline>::value;

	GlyphRunShapingMemory backup {};
	if (!mem) mem = &backup;
	
	//
	// convert the text to utf-32
	//
	usize requiredUtf32Size = 0;
	ToUtf32(text, {}, &requiredUtf32Size);
	
	PrepareShapingMemory(mem, requiredUtf32Size);
	
	if (!ToUtf32(text, {mem->utf32Text, mem->capacity}, &mem->size)) {
		LogError("ToUtf32() failed");
		return false;
	}
	
	ASSERT(mem->size == requiredUtf32Size);
	
	//
	// get the indicies and their design-advances
	//
	
	PrepareGlyphRun(run, mem->size);

	if (HRESULT hr = font.fontFace->GetGlyphIndices(
			mem->utf32Text,
			static_cast<u32>(mem->size),
			run->indicies); hr != S_OK) {
		LogError("GetGlyphIndices() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	if (HRESULT hr = font.fontFace->GetDesignGlyphAdvances(
			static_cast<u32>(mem->size),
			run->indicies,
			mem->designGlyphAdvances); hr != S_OK) {
		LogError("GetDesignGlyphAdvances() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	//
	// convert to the actual advances
	//
	float totalAdvance = 0.0;
	const float advanceSpace = font.GetSpaceAdvance();

	for (u64 i = 0; i < mem->size; i++) {
		
		if constexpr (IS_MULTILINE) {
		
			if (mem->utf32Text[i] == U'\n' || mem->utf32Text[i] == U'\r') {
				run->indicies[i] = font.glyphIndexSpace;
				run->advances[i] = 0.0f;
				
				const bool isNewline     = mem->utf32Text[i] == U'\n';
				const bool isCariageRet  = mem->utf32Text[i] == U'\r';
				const bool isNextNewline = (i+1) < mem->size && mem->utf32Text[i+1] == '\n';
				
				if (isNewline || (isCariageRet && !isNextNewline))
					run->linebreaks.push_back(i);
					
				continue;
			}
		}
		
		if (mem->utf32Text[i] == U' ') {
			run->indicies[i] = font.glyphIndexSpace;
			run->advances[i] = advanceSpace;
			totalAdvance    += advanceSpace;

		} else if (mem->utf32Text[i] == U'\t') {
			
			// @TODO(settings) make tabstop-width configurable
			const float tabStopWidth = advanceSpace * 4;
			
			// @TODO is this fixed???
			const int numCurrentTabstops = static_cast<int>(totalAdvance / tabStopWidth);
			const float distNextTabstop = ((numCurrentTabstops + 1) * tabStopWidth);

			const float advance = static_cast<float>(distNextTabstop - totalAdvance);

			run->indicies[i] = font.glyphIndexSpace;
			run->advances[i] = advance;
			totalAdvance    += advance;

		} else {
			const float advance = font.ConvertFromDesignUnits(mem->designGlyphAdvances[i]);
			run->advances[i] = advance;
			totalAdvance    += advance;
		}
	}

	run->size = mem->size;
	
	if constexpr (IS_MULTILINE) {
		if (!text.empty()) run->linebreaks.push_back(run->size);
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool GlyphRun::Shape(std::string_view text, const Font& font, GlyphRunShapingMemory* mem /*= nullptr*/) {
	return ShapeInternal<GlyphRun>(this, text, font, mem);
}

u64 GlyphRun::Hittest(f32 offset) const {

	float totalAdvance = 0.f;
	
	for (u64 i = 0; i < size; i++) {

		const float advance = advances[i];
		
		// the hit only counts if we are in the first half of the current glyph,
		// otherwise the hit is on the next glyph
		// this just feels nicer because you usually don't aim for single letters
		// with the cursor but rather for the gaps between them
		if (offset < (totalAdvance + (advance * .5f)))
			return i;

		totalAdvance += advance;
	}

	// we are outside - we return one past the end
	return size;
}

float GlyphRun::GetGlyphOffset(u64 codepoint) const {

	float advanced = 0.f;
	for (u64 i = 0; i < codepoint; i++)
		advanced += advances[i];

	return advanced;
}

void GlyphRun::GetGlyphOffsetRange(u64 from, u64 to, /*out*/ float* fromOffset, /*out*/ float* toOffset) const {

	ASSERT(from <= to)

	float advanced = 0.f;
	for (u64 i = 0; i < from; i++)
		advanced += advances[i];

	*fromOffset = advanced;

	for (u64 i = from; i < to; i++)
		advanced += advances[i];

	*toOffset = advanced;
}

float GlyphRun::GetTotalAdvance() const {
	return GetGlyphOffset(size);
}

void GlyphRun::Draw(ID2D1RenderTarget* renderTarget, const D2D_POINT_2F& pos, const Font& font, ID2D1Brush* textBrush) const {
	if (size == 0) return;

	const DWRITE_GLYPH_RUN glyphRun {
		.fontFace      = font.fontFace,
		.fontEmSize    = font.size,
		.glyphCount    = static_cast<u32>(size),
		.glyphIndices  = indicies,
		.glyphAdvances = advances,
		.glyphOffsets  = nullptr,
		.isSideways    = FALSE,
		.bidiLevel     = 0 };

	renderTarget->DrawGlyphRun(
		D2D_POINT_2F {
			.x = pos.x,
			.y = pos.y + font.baselineOffset },
		&glyphRun,
		textBrush);
		
		//DWRITE_MEASURING_MODE_GDI_NATURAL);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool GlyphRunMultiline::Shape(std::string_view text, const Font& font, GlyphRunShapingMemory* mem /*= nullptr*/) {
	linebreaks.clear();
	return ShapeInternal<GlyphRunMultiline>(this, text, font, mem);
	
}

static void DrawLine(const GlyphRunMultiline* self, ID2D1RenderTarget* renderTarget, D2D_POINT_2F pos, const Font& font, ID2D1Brush* textBrush, u64 lineStart, u64 lineEnd) {
	ASSERT(lineStart <= self->size);
		
	const f32* lineAdvanves = self->advances + lineStart;
	const u16* lineIndicies = self->indicies + lineStart;
	const s64  lineSize = lineEnd - lineStart;
	ASSERT(lineSize >= 0);
		
	const DWRITE_GLYPH_RUN glyphRun {
		.fontFace      = font.fontFace,
		.fontEmSize    = font.size,
		.glyphCount    = static_cast<u32>(lineSize),
		.glyphIndices  = lineIndicies,
		.glyphAdvances = lineAdvanves,
		.glyphOffsets  = nullptr,
		.isSideways    = FALSE,
		.bidiLevel     = 0 };
	
	renderTarget->DrawGlyphRun(pos, &glyphRun, textBrush);
}

void GlyphRunMultiline::Draw(ID2D1RenderTarget* renderTarget, const D2D_POINT_2F& pos, const Font& font, ID2D1Brush* textBrush, f32 extraOffsetFirstLine /*= 0.0f*/) const {
	if (size == 0) return;
	
	D2D_POINT_2F position {
		.x = pos.x,
		.y = pos.y + font.baselineOffset };
		
	u64 lineStart = 0u;
	for (u64 i = 0u; i < linebreaks.size(); i++) {	
		const u64 lineEnd = linebreaks[i];
		
		D2D_POINT_2F actualPosition = position;
		if (i == 0u)
			actualPosition.x += extraOffsetFirstLine;
				
		DrawLine(this,
			renderTarget,
		    position,
		    font,
		    textBrush,
		    lineStart,
		    lineEnd);
		
		position.y += font.lineHeight;
		lineStart = lineEnd + 1;
	}
}

u64 GlyphRunMultiline::GetLineCount() const {
	return linebreaks.size();
}

f32 GlyphRunMultiline::GetMaxAdvance(f32 extraOffsetFirstLine /*= 0.0f*/) const {
	
	f32 maxAdvance = 0.0f;
	u64 lineStart = 0u;
	for (u64 i = 0u; i < linebreaks.size(); i++) {	
		const u64 lineEnd = linebreaks[i];
	
		f32 currentAdvance = 0.0f;
		if (i == 0u)
			currentAdvance += extraOffsetFirstLine;
		
		for (u64 i = lineStart; i < lineEnd; i++)
			currentAdvance += advances[i];
		
		lineStart = lineEnd + 1;
		if (maxAdvance < currentAdvance)
			maxAdvance = currentAdvance;
	}
	
	return maxAdvance;
}

f32 GlyphRunMultiline::GetAdvanceFirst() const {
	const u64 end = linebreaks.empty()
		? size
		: linebreaks.front();
	
	f32 totalAdvance = 0.0f;
	for (u64 i = 0u; i < end; i++) {
		totalAdvance = advances[i];
	}
	
	return totalAdvance;
}