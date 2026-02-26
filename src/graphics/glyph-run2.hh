#pragma once
#include "basic.hh"

#include <memory>
#include <string_view>
#include <span>

struct Font;
struct ID2D1RenderTarget;
struct ID2D1SolidColorBrush;

struct GlyphRun2 {
	
	//------------------------------------------
	// data
	//------------------------------------------
	
	f32* advances    = nullptr;
	u16* indicies    = nullptr;
	u32* mapping     = nullptr;
	
	f32 totalAdvanve = 0.0f;
	u64 size = 0u;
	u64 capacity = 0u;
	std::unique_ptr<s8[]> memory = nullptr;
	
	//------------------------------------------
	// functions
	//------------------------------------------	
	
	// static void ShapeBatch(std::span<GlyphRun2>, std::span<std::string_view> texts);
	
	bool Shape(std::string_view text, const Font& font);
	
	u64  HitTest(f32 offset) const;
	f32  MeasureOffset(u64 pos) const;
	void MeasureOffsetRange(u64 from, u64 to, f32* offFrom, f32* offTo) const;
	
	void Draw(ID2D1RenderTarget* renderTarget, f32 x, f32 y, const Font& font, ID2D1SolidColorBrush* brush) const;
	void DrawCenter(ID2D1RenderTarget* renderTarget, f32 x, f32 y, f32 availableW, const Font& font, ID2D1SolidColorBrush* brush) const;
};
