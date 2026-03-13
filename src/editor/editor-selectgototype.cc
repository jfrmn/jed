#include "editor-selectgototype.hh"
#include "editor.hh"
#include "globals.hh"
#include "events.hh"
#include "theme.hh"
#include "editor-textlocationlist.hh"

#include "ui/constants.h"

#include "util/rect-util.hh"
#include "graphics/effects.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

static const EditorSelectGotoType::Item ITEMS[] {
	EditorSelectGotoType::Item {
		.label = "Decleration",
		.GotoFunction = &Language::GotoDecleration},
	EditorSelectGotoType::Item {
		.label = "Definition",
		.GotoFunction = &Language::GotoDefinition},
	EditorSelectGotoType::Item {
		.label = "Type Definition",
		.GotoFunction = &Language::GotoTypeDefinition},
	EditorSelectGotoType::Item {
		.label = "Implementation",
		.GotoFunction = &Language::GotoImplementation}
};

static const u64 ITEM_COUNT = STATIC_ARRAY_SIZE(ITEMS);

EditorSelectGotoType* EditorSelectGotoType::Make(Editor* owner) {
	auto self = new EditorSelectGotoType();
	self->owner = owner;
	self->references = 1;
	return self;
}

void EditorSelectGotoType::OnUpdate() {
		
	const D2D1_POINT_2F curPos = owner->GetCaretLocation();
	const D2D1_POINT_2F position {
		.x = curPos.x,
		.y = curPos.y + theme.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y };
	
	staticGlyphRun.Shape("Goto...", theme.fontUi);
	
	f32 width = .0f;
	GlyphRun runItems[ITEM_COUNT];
	for (u64 i = 0u; i < ITEM_COUNT; i++) {
		runItems[i].Shape(ITEMS[i].label, theme.fontUi);
		
		const f32 currentWidth = runItems[i].width + PADDING_X2;
		if (width < currentWidth)
			width = currentWidth;
	}
	
	const f32 height = (ITEM_COUNT + 1) * theme.fontUi.lineHeight;
	
	//
	// draw background
	//
	{	
		const D2D_RECT_F area = 	MakeRect(position.x, position.y, width, height);
		BlurArea(deviceContext, area);
	}
	
	//
	// draw header
	//
	{
		deviceContext->FillRectangle(MakeRect(position.x, position.y, width, theme.fontUi.lineHeight), theme.GetBrushUiBackground());
		staticGlyphRun.Draw(deviceContext, position.x + PADDING, position.y, theme.fontUi, theme.GetBrushUiText());
	}
	
	//
	// draw items
	//
	for (u64 i = 0u; i < ITEM_COUNT; i++) {
		
		const f32 posY = position.y + (theme.fontUi.lineHeight * (i + 1));
		if (i == selectedItem)
			deviceContext->FillRectangle(MakeRect(position.x, posY, width, theme.fontUi.lineHeight), theme.GetBrushSelection());
	
		runItems[i].Draw(deviceContext, position.x + PADDING, posY, theme.fontUi, theme.GetBrushUiText());
	}			
}

bool EditorSelectGotoType::OnKeyDown(KeyEvent event) {

	if ((event.vkeycode == VK_DOWN || event.vkeycode == VK_UP) && event.NoModifiers()) {
		
		selectedItem = event.vkeycode == VK_DOWN ?
			IncrementWrapAround(selectedItem, static_cast<u64>(ITEM_COUNT)):
			DecrementWrapAround(selectedItem, static_cast<u64>(ITEM_COUNT));
		
		return true;
	
	} else if ((event.vkeycode == VK_RETURN) && event.NoModifiers()) {
		if (!owner->language) return true;
		
		ASSERT(selectedItem >= 0u && selectedItem < ITEM_COUNT);
		const Item& item = ITEMS[selectedItem];
		
		EditorTextLocationList* textLocationList = (owner->language->*(item.GotoFunction))(owner);
		owner->editorCaretAttached = textLocationList;
		RemoveReference();
		return true;
	}
	
	return false;	
}

void EditorSelectGotoType::OnInput() {
	owner->editorCaretAttached = nullptr;
	RemoveReference();
}
