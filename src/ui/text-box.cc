#include "text-box.hh"
#include "basic.hh"
#include "events.hh"
#include "settings.hh"
#include "graphics.hh"

#include "ui/constants.h"

#include "logging.hh"
#include "util.hh"

bool TextBox::Init(Font* fontToUse, std::string_view placeholderText /*= {}*/, std::string initalText /*= {}*/) {

	this->font = fontToUse;
	this->placeholderText = placeholderText;

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
	
	TextChange* change = textController.NewTextChange();
	
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

void TextBox::Update() {

	const D2D_RECT_F area = GetArea();

	//
	// fill background
	//
	{
		Color backgroundColor;
		if      (invalid)  backgroundColor = settings.colors.uiBackgroundInvalid;
		else if (inactive) backgroundColor = settings.colors.uiBackgroundInactive;
		else               backgroundColor = settings.colors.uiBackground;
		
		deviceContext->FillRoundedRectangle(ToRounded(area), UseColor(backgroundColor));
	}

	//
	// draw text or placeholder
	//
	if (const std::string_view text = GetText(); !text.empty()) {
		glyphRun.ShapeAndDraw(deviceContext,
			text,
			position.x + PADDING,
			position.y + PADDING,
			*font,
			settings.colors.UseUiText());
	
	} else {
		staticGlyphRun.ShapeAndDraw(deviceContext,
			placeholderText,
			position.x + PADDING,
			position.y + PADDING,
			*font,
			settings.colors.UseUiText(false));
	}

	// draw cursor
	if (!inactive) {

		ASSERT(textController.carets.front().position.line == 0);
		const float offsetCursor = glyphRun.MeasureOffset(textController.carets.front().position.character);
		
		deviceContext->FillRectangle(
			D2D_RECT_F {
				.left   = position.x + PADDING + offsetCursor,
				.top    = position.y + PADDING,
				.right  = position.x + PADDING + offsetCursor + 2.0f,
				.bottom = position.y + PADDING + font->lineHeight},
			settings.colors.UseUiText());
	}

	// draw selection
	if (textController.HasSelection()) {

		TextPosition selectionStart, selectionEnd;
		textController.GetSelection(&selectionStart, &selectionEnd);

		f32 offsetStart, offsetEnd;
		glyphRun.MeasureOffsetRange(selectionStart.character, selectionEnd.character, &offsetStart, &offsetEnd);

		deviceContext->FillRectangle(
			D2D_RECT_F {
				.left   = position.x + PADDING + offsetStart,
				.top    = position.y + PADDING,
				.right  = position.x + PADDING + offsetEnd,
				.bottom = position.y + PADDING + font->lineHeight},
			settings.colors.UseSelection(!inactive));
	}	
}

void TextBox::ClearText() {
	textController.Reset();
}

TextBox::HandleEventResult TextBox::HandleEvent(const Event& event) {
	
	if (event.type == Event::Type_KeyPress && event.keypress.vkc == VK_RETURN)
		return HandleEventResult {.handled = true, .changed = false};
	
	TextChange* change = nullptr;
	const bool handled = textController.HandleEvent(event, &change);
	return HandleEventResult {.handled = handled, .changed = change != nullptr};
}
