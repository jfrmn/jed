#include "editor-caretattached.hh"
#include "editor.hh"
#include "settings.hh"

#include "ui/constants.h"

#include "graphics/effects.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

void EditorCaretAttached::AddReference() {
	++references;
}

void EditorCaretAttached::RemoveReference() {
	if ((--references) <= 0) {
		delete this;
	}
}

bool EditorCaretAttached::DrawIncompleteState(ID2D1DeviceContext* deviceContext, std::string_view noItemsText /*= "No items.*/) {
	if (state == State_Completed) return false;	
	
	const D2D_POINT_2F position = GetPosition();
		
	std::string_view text;
	ID2D1Bitmap* icon = nullptr;
	if (state == State_Unknown)  {
		text = "<unknown>";
		icon = settings.icons.unknown;
	
	} else if (state == State_Fetching) {
		text = "fectching...";
		icon = settings.icons.waiting;
	
	} else if (state == State_Errored) {
		text = error;
		icon = settings.icons.error;
	
	} else if (state == State_NoItems) {
		text = noItemsText;
		icon = settings.icons.noItems;
	
	} else {
		ASSERT_UNREACHABLE;
	}
	
	staticGlyphRun.Shape(text, settings.fontEditor);
	
	const f32 width = PADDING_X3 + settings.fontEditor.lineHeight + staticGlyphRun.width;
	
	// background
	BlurArea(
		deviceContext, 
		D2D_RECT_F {
			.left   = position.x,
			.top    = position.y,
			.right  = position.x + width,
			.bottom = position.y + settings.fontEditor.lineHeight});

	// icon
	deviceContext->DrawBitmap(
		icon,
		D2D_RECT_F {
			.left   = position.x + PADDING,
			.top    = position.y,
			.right  = position.x + PADDING + settings.fontEditor.lineHeight,
			.bottom = position.y + settings.fontEditor.lineHeight});

	// text
	staticGlyphRun.Draw(deviceContext,
		position.x + PADDING_X2 + settings.fontEditor.lineHeight,
		position.y,
		settings.fontEditor,
		settings.GetBrushUiText());
	
	return true;
}

D2D_POINT_2F EditorCaretAttached::GetPosition() const {
	const D2D1_POINT_2F cursorPosition = owner->GetCaretLocation();
	return D2D_POINT_2F {
		.x = cursorPosition.x,
		.y = cursorPosition.y + settings.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y };
}