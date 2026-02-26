#include "editor-caretattached.hh"
#include "editor.hh"

#include "ui/style.hh"
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
	Style::Icon icon;
	if (state == State_Unknown)  {
		text = "<unknown>";
		icon = Style::Icon_Unknown;
	
	} else if (state == State_Fetching) {
		text = "fectching...";
		icon = Style::Icon_Waiting;
	
	} else if (state == State_Errored) {
		text = error;
		icon = Style::Icon_Error;
	
	} else if (state == State_NoItems) {
		text = noItemsText;
		icon = Style::Icon_NoItems;
	
	} else {
		ASSERT_UNREACHABLE;
	}
	
	GlyphRun run;
	run.Shape(text, style.fontEditor, &owner->glyphRunShapingMemory);
	
	const f32 width = PADDING_X3 + style.fontEditor.lineHeight + run.GetTotalAdvance();
	
	// background
	BlurArea(
		deviceContext, 
		D2D_RECT_F {
			.left   = position.x,
			.top    = position.y,
			.right  = position.x + width,
			.bottom = position.y + style.fontEditor.lineHeight});

	// icon
	deviceContext->DrawBitmap(
		style.icons[icon],
		D2D_RECT_F {
			.left   = position.x + PADDING,
			.top    = position.y,
			.right  = position.x + PADDING + style.fontEditor.lineHeight,
			.bottom = position.y + style.fontEditor.lineHeight});

	// text
	run.Draw(deviceContext,
		D2D1_POINT_2F {
			.x = position.x + PADDING_X2 + style.fontEditor.lineHeight,
			.y = position.y},
		style.fontEditor,
		style.GetBrushUiText());
	
	return true;
}

D2D_POINT_2F EditorCaretAttached::GetPosition() const {
	const D2D1_POINT_2F cursorPosition = owner->GetCaretLocation();
	return D2D_POINT_2F {
		.x = cursorPosition.x,
		.y = cursorPosition.y + style.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y };
}