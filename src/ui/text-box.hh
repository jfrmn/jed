#pragma once
#include "text/text-controller.hh"
#include "graphics/glyph-run.hh"
#include "graphics/font.hh"

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

struct ID2D1RenderTarget;
struct KeyEvent;

struct TextBox {

	//-------------------------------------------
	// data

	Font* font = nullptr;

	D2D_POINT_2F position = {};
	float width = .0f;

	TextController textController = {};	
	GlyphRun glyphRun = {};
	
	std::string_view placeholderText = {};

	bool invalid = false;
	bool inactive = false;
	
	//-------------------------------------------
	// functions

	bool Init(Font* fontToUse, std::string_view placeholderText = {}, std::string initalText = {});

	std::string_view GetText() const;
	void SetText(std::string_view newText);
	void ClearText();

	D2D_RECT_F GetArea() const;
	float Height() const;

	void OnUpdate();
	
	// returns whether the text changed or not
	bool OnKeyDown(KeyEvent event, bool* changed = nullptr);
	
	// returns whether the text changed or not
	bool OnChar(const char* data, u64 len);
};