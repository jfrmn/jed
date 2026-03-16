#include "editor-diagnosticslist.hh"
#include "globals.hh"
#include "events.hh"
#include "settings.hh"

#include "editor/editor.hh"
#include "editor/editor-diagnostics.hh"

#include "ui/constants.h"

#include "graphics/glyph-run.hh"
#include "graphics/effects.hh"

#include "util/rect-util.hh"
#include "util/format.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr float ITEM_HIGHLIGHT_OPACITY_VALUE_MAX = (F32_PI * 2.0f) * 10.0f; // 10 cylces
static constexpr float ITEM_HIGHLIGHT_OPACITY_SPEED = 0.004f;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorDiagnosticsList* EditorDiagnosticsList::Make(Editor* editor) {
	auto self = new EditorDiagnosticsList();
	self->owner = editor;
	return self;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void ActionGotoItem(EditorDiagnosticsList* self, u64 itemIndex, bool close) {
	const std::scoped_lock lock {self->owner->editorDiagnostics.mutex};
		
	// NOTE: this can happen if new diagnostics have been published since the last OnUpdate()
	// The language server publishes them asynchronusly/in a different thread
	if (itemIndex >= self->owner->editorDiagnostics.RecordCount())
		return;
		
	const EditorDiagnostics::Record& record = self->owner->editorDiagnostics.records[itemIndex];
	self->owner->textController.Select(record.from, record.to);
	self->owner->ScrollToLine(record.from.line);
	
	// if control is pressed we close ourself
	if (close) {
		ASSERT(self->owner->toolWindow == self);
		self->owner->toolWindow = nullptr;
		delete self;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct Item {
	GlyphRun code = {};
	GlyphRunMultiline message = {};
	ID2D1Bitmap* icon = nullptr;
	D2D_SIZE_F size = {};
};

static void OnClickItem(void* ud, u64 i) {
	auto self = static_cast<EditorDiagnosticsList*>(ud);
	ActionGotoItem(self, i, false);	
}

void EditorDiagnosticsList::OnUpdate() {

	//
	// update animation
	//
	{
		itemHighlightAnimationValue += ITEM_HIGHLIGHT_OPACITY_SPEED * deltaTime;
		if (itemHighlightAnimationValue > ITEM_HIGHLIGHT_OPACITY_VALUE_MAX)
			itemHighlightAnimationValue = ITEM_HIGHLIGHT_OPACITY_VALUE_MAX;
		else needsUpdate = true;
	}
	
	f32 totalWidth = RectWidth(owner->area) * 0.3f + PADDING + MARGIN_X2;
	f32 totalHeight = settings.fontUi.lineHeight + MARGIN_X2;
	
	Item* items = nullptr;
	DEFER(delete[] items);
	
	//
	// shape the text, calc size
	//
	{
		const std::scoped_lock lock {owner->editorDiagnostics.mutex};
		itemCount = owner->editorDiagnostics.RecordCount();
		items = new Item[itemCount];
		
		for (u64 i = 0u; i < itemCount; i++) {
			
			Item& item = items[i];
			const EditorDiagnostics::Record& record = owner->editorDiagnostics.records[i];
			
			// shape
			
			item.code.Shape(record.code, settings.fontEditor);
			item.message.Shape(record.message, settings.fontUi);
			
			// select icon
			item.icon = *Diagnostics::SEVERITY_ICONS[record.severity];
			
			// measure the width and height
			
			item.size.width = std::max(
				item.code.width + settings.fontEditor.lineHeight + PADDING_X3,
				item.message.GetWidth() + PADDING_X2);
			
			item.size.height = PADDING_X2 + settings.fontEditor.lineHeight
	             			 + (settings.fontUi.lineHeight * item.message.LineCount());
			
			if (totalWidth < item.size.width)
				totalWidth = item.size.width;
				
			totalHeight += item.size.height;
		}
	}
		
	const D2D_RECT_F area {
		.left   = owner->area.right - MARGIN - SCROLLBAR_WIDTH_WIDE - totalWidth,
		.top    = owner->area.top   + MARGIN,
		.right  = owner->area.right - MARGIN - SCROLLBAR_WIDTH_WIDE,
		.bottom = owner->area.top   + MARGIN + totalHeight};	
	
	//
	// background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, area);
		if (!background) return;
		DEFER(background->Release());
	
		DrawGlow(deviceContext, background, area);
	
		PushLayer(deviceContext, area);
		BlurArea(deviceContext, area, background);
		PopLayer(deviceContext);
	}
	
	//DEFER(PopLayer(deviceContext));
	
	//
	// draw header
	//
	{
		staticGlyphRun.Shape("Diagnostics", settings.fontUi);
		staticGlyphRun.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, settings.fontUi, settings.GetBrushUiText());
		
		// underline
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = area.left + MARGIN,
				.y = area.top  + MARGIN + settings.fontUi.underlineOffset },
			D2D1_POINT_2F {
				.x = area.left + MARGIN + staticGlyphRun.width,
				.y = area.top  + MARGIN + settings.fontUi.underlineOffset },
			settings.GetBrushUiText());
		
		
		const f32 offset = PADDING + staticGlyphRun.width;
		
		char buffer[32] {'\0'};
		const u64 size = FormatToBuffer(buffer, "% records", itemCount);
		staticGlyphRun.Shape({buffer, size}, settings.fontUi);
		staticGlyphRun.Draw(deviceContext, area.left + MARGIN + offset, area.top + MARGIN, settings.fontUi, settings.GetBrushUiText(false));
	}
	
	//
	// draw records
	//
	f32 posY = area.top + settings.fontUi.lineHeight + MARGIN_X2;
	for (u64 i = 0u; i < itemCount; i++) {
		
		Item& item = items[i];
		const D2D_RECT_F itemArea = MakeRect(area.left, posY, totalWidth, item.size.height);
		
		if (i == selectedItem) {
			ID2D1SolidColorBrush* brushGlow = settings.GetBrushDropShadow();
			const f32 opacityBefore = brushGlow->GetOpacity();
			DEFER(brushGlow->SetOpacity(opacityBefore));
			
			const f32 opacity = std::sin(itemHighlightAnimationValue) * 0.4f + 0.5f;
			brushGlow->SetOpacity(opacity);
			
			deviceContext->FillRectangle(itemArea, brushGlow);
		}
			
		deviceContext->DrawBitmap(
			item.icon,
			MakeRect(
				itemArea.left + PADDING,
				itemArea.top + PADDING,
		    	settings.fontEditor.lineHeight,
		    	settings.fontEditor.lineHeight));
				
		item.code.Draw(deviceContext,
			itemArea.left + PADDING_X2 + settings.fontEditor.lineHeight,
			itemArea.top + PADDING,
			settings.fontEditor,
			settings.GetBrushUiText());
		
		item.message.Draw(deviceContext,
			itemArea.left + PADDING,
			itemArea.top + PADDING + settings.fontEditor.lineHeight,
			settings.fontUi,
			settings.GetBrushUiText(false));
		
		if (mouse.Hittest(itemArea, this, OnClickItem, i)) {
			deviceContext->FillRectangle(itemArea, settings.GetBrushHover(mouse.isDown));	
		}
			
		posY += item.size.height;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorDiagnosticsList::OnKeyDown(KeyEvent event) {
	if ((event.vkeycode == VK_UP || event.vkeycode == VK_DOWN) && event.NoModifiers()) {
		
		selectedItem = event.vkeycode == VK_DOWN
			? IncrementWrapAround(selectedItem, itemCount)
			: DecrementWrapAround(selectedItem, itemCount);
		
		return true;
	
	} else if (event.vkeycode == VK_RETURN) {
		ActionGotoItem(this, selectedItem, event.ctrl);			
		return true;
	}
	
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorDiagnosticsList::IsDiagnosticsList() const {
	return true;
}