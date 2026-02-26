#include "text-box.hh"
#include "basic.hh"
#include "globals.hh"
#include "events.hh"

#include "ui/constants.h"
#include "ui/style.hh"

#include "util/logging.hh"
#include "util/rect-util.hh"

bool TextBox::Init(Font* fontToUse, std::string_view placeholderText /*= {}*/, std::string initalText /*= {}*/) {

	this->font = fontToUse;

	if (!placeholderText.empty()) {

		if (!glyphRunPlaceholder.Shape(placeholderText, *fontToUse)) {
			LogError("shaping placeholder glyph run failed");
			return false;
		}
	}

	if (!textController.InitForTextbox(std::move(initalText))) {
		LogError("init text-controller failed");
		return false;
	}

	return true;
}

std::string_view TextBox::GetText() const {
	return textController.buffer.GetLineAt(0).GetText();
}

void TextBox::SetText(std::string_view text) {
	textController.SetCaretPosition(TextPosition {0u, text.size()});
	
	TextChange* change = nullptr;
	textController.InitTextChange(&change);
	
	TextChangeOperation* operation = change->NewOperation();
	textController.buffer.RemoveInLine(0u, 0u, textController.buffer.GetLineAt(0).length, operation);
	textController.buffer.InsertInLine({0u, 0u}, text, operation);
	
	// @FIXME is incomplete
}

D2D_RECT_F TextBox::GetArea() const {
	return D2D_RECT_F {
		.left   = position.x,
		.top    = position.y,
		.right  = position.x + width,
		.bottom = position.y + Height() };
}

float TextBox::Height() const {
	return (font->lineHeight + PADDING_X2);
}

void TextBox::OnUpdate() {

	const D2D_RECT_F area = GetArea();

	//
	// fill background
	//
	{
		ID2D1SolidColorBrush* brush = nullptr;
		if      (invalid)  brush = style.GetBrushUiBackgroundInvalid();
		else if (inactive) brush = style.GetBrushUiBackground(false);
		else               brush = style.GetBrushUiBackground(true);
		
		const D2D1_ROUNDED_RECT roundedArea = MakeRoundedRect(area, RADIUS);
		deviceContext->FillRoundedRectangle(roundedArea, brush);
	}

	//
	// draw text or placeholder
	//
	if (const std::string_view text = GetText(); !text.empty()) {
		glyphRun.Shape(text, *font);
		glyphRun.Draw(deviceContext,
			D2D1_POINT_2F {
				.x = position.x + PADDING,
				.y = position.y + PADDING },
			*font,
			style.GetBrushUiText());
	
	} else {
		glyphRunPlaceholder.Draw(deviceContext,
			D2D1_POINT_2F {
				.x = position.x + PADDING,
				.y = position.y + PADDING },
			*font,
			style.GetBrushUiText(false));
	}

	// draw cursor
	if (!inactive) {

		ASSERT(textController.carets.front().position.line == 0);
		const float offsetCursor = glyphRun.GetGlyphOffset(textController.carets.front().position.column);
		
		deviceContext->FillRectangle(
			D2D_RECT_F {
				.left   = position.x + PADDING + offsetCursor,
				.top    = position.y + PADDING,
				.right  = position.x + PADDING + offsetCursor + 2.0f,
				.bottom = position.y + PADDING + font->lineHeight},
			style.GetBrushUiText());
	}

	// draw selection
	if (textController.HasSelection()) {

		TextPosition selectionStart, selectionEnd;
		textController.GetSelection(&selectionStart, &selectionEnd);

		float offsetStart, offsetEnd;
		glyphRun.GetGlyphOffsetRange(selectionStart.column, selectionEnd.column, &offsetStart, &offsetEnd);

		deviceContext->FillRectangle(
			D2D_RECT_F {
				.left   = position.x + PADDING + offsetStart,
				.top    = position.y + PADDING,
				.right  = position.x + PADDING + offsetEnd,
				.bottom = position.y + PADDING + font->lineHeight},
			style.GetBrushSelection(!inactive));
	}	
}

void TextBox::ClearText() {
	textController.Reset();
}

bool TextBox::OnKeyDown(KeyEvent event, bool* changed) {

	if (event.vkeycode == VK_RETURN)
		return false;

	TextChange* change = nullptr;
	textController.OnKeyDown(event, &change);
	return (change != nullptr);
}

bool TextBox::OnChar(const char* data, u64 len) {	
	TextChange* change = nullptr;
	textController.OnChar(data, len, &change);
	return (change != nullptr);
}
