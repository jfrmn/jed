#include "editor-diagnosticslist.hh"
#include "events.hh"
#include "glyph-run.hh"
#include "graphics.hh"
#include "settings.hh"
#include "util.hh"

#include "editor/editor.hh"
#include "editor/editor-diagnostics.hh"

#include "ui/constants.h"
#include "ui/animation.hh"
#include "ui/icons.hh"

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
	self->owner->textController.SetSelection(record.from, record.to);
	self->owner->ScrollToLine(record.from.line);
	
	// if control is pressed we close ourself
	if (close) {
		ASSERT(self->owner->toolWindow == self);
		self->owner->toolWindow = nullptr;
		delete self;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnClickItem(void* ud, u64 i) {
	auto self = static_cast<EditorDiagnosticsList*>(ud);
	ActionGotoItem(self, i, false);	
}

void EditorDiagnosticsList::Update() {

	// update animation
	AnimationCycling::Advance(&itemHighlightAnimationValue);
	
	//
	// update items
	//
	if (diagnosticsVersion != owner->editorDiagnostics.diagnosticsVersion) {
		
		const std::scoped_lock lock {owner->editorDiagnostics.mutex};
		items.clear();
		items.reserve(owner->editorDiagnostics.RecordCount());
		
		for (u64 i = 0u; i < owner->editorDiagnostics.RecordCount(); i++) {
			
			Item& item = items.emplace_back();
			const EditorDiagnostics::Record& record = owner->editorDiagnostics.records[i];
			
			// shape
			item.code.Shape(record.code, settings.fontEditor);
			item.message.Shape(record.message, settings.fontUi);
			
			// set severity
			item.severity = record.severity;
			
			// measure the width and height
			item.width = std::max(
				item.code.width + settings.fontEditor.lineHeight + PADDING_X3,
				item.message.GetWidth() + PADDING_X2);
			
			item.height = PADDING_X2 + settings.fontEditor.lineHeight
	                    + (settings.fontUi.lineHeight * item.message.LineCount());	
		}
		
		selectedItem = std::min(selectedItem, items.size()-1u);
		diagnosticsVersion = owner->editorDiagnostics.diagnosticsVersion;
	}
	
	//
	// calc totalWidth and height
	//
	const f32 maxWidth = RectWidth(owner->area) - MARGIN_X2;
	f32 totalWidth = RectWidth(owner->area) * 0.3f + PADDING + MARGIN_X2;
	f32 totalHeight = settings.fontUi.lineHeight + MARGIN_X2;	
	
	for (const Item& item : items) {
		if (totalWidth < item.width)
			totalWidth = std::min(maxWidth, item.width);
		totalHeight += item.height;
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
	}
	DEFER(PopLayer(deviceContext));
	
	//
	// draw header
	//
	{
		staticGlyphRun.Shape("Diagnostics", settings.fontUi);
		staticGlyphRun.Draw(deviceContext, area.left + MARGIN, area.top + MARGIN, settings.fontUi, settings.colors.UseUiText());
		
		// underline
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = area.left + MARGIN,
				.y = area.top  + MARGIN + settings.fontUi.underlineOffset },
			D2D1_POINT_2F {
				.x = area.left + MARGIN + staticGlyphRun.width,
				.y = area.top  + MARGIN + settings.fontUi.underlineOffset },
			settings.colors.UseUiText());
		
		
		const f32 offset = PADDING + staticGlyphRun.width;
		
		char buffer[32] {'\0'};
		const int size = sprintf_s(buffer, "%zu records", items.size());
		ASSERT_SOFT(size > 0);
		
		staticGlyphRun.Shape({buffer, static_cast<u64>(size)}, settings.fontUi);
		staticGlyphRun.Draw(deviceContext, area.left + MARGIN + offset, area.top + MARGIN, settings.fontUi, settings.colors.UseUiText(false));
	}
	
	//
	// draw records
	//
	f32 posY = area.top + settings.fontUi.lineHeight + MARGIN_X2;
	for (u64 i = 0u; i < items.size(); i++) {
		
		Item& item = items[i];
		const D2D_RECT_F itemArea = MakeRect(area.left, posY, totalWidth, item.height);
		
		if (i == selectedItem) {
			ID2D1SolidColorBrush* brush = settings.colors.UseDropShadow();
			const f32 opacityBefore = brush->GetOpacity();
			DEFER(brush->SetOpacity(opacityBefore));
			
			const f32 opacity = std::sin(itemHighlightAnimationValue) * 0.4f + 0.5f;
			brush->SetOpacity(opacity);
			
			deviceContext->FillRectangle(itemArea, brush);
		}
		
		icons.DrawIcon(
			deviceContext,
			Diagnostics::ICONS[item.severity],
			D2D_POINT_2F {
				.x = itemArea.left + PADDING,
				.y = itemArea.top + PADDING},
	    	settings.fontEditor.lineHeight,
		    Diagnostics::COLORS[item.severity]);
				
		item.code.Draw(deviceContext,
			itemArea.left + PADDING_X2 + settings.fontEditor.lineHeight,
			itemArea.top + PADDING,
			settings.fontEditor,
			settings.colors.UseUiText());
		
		item.message.Draw(deviceContext,
			itemArea.left + PADDING,
			itemArea.top + PADDING + settings.fontEditor.lineHeight,
			settings.fontUi,
			settings.colors.UseUiText(false));
		
		if (mouse.Hittest(itemArea, this, OnClickItem, i)) {
			deviceContext->FillRectangle(itemArea, settings.colors.UseHover(mouse.isDown));	
		}
			
		posY += item.height;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorDiagnosticsList::HandleEvent(const Event& event) {
	if (event.type == Event::Type_KeyPress) {
		if ((event.keypress.vkc == VK_UP || event.keypress.vkc == VK_DOWN) && event.keypress.mods == KM_None) {
			if (items.empty()) return true;
			
			selectedItem = event.keypress.vkc == VK_DOWN
				? IncrementWrapAround(selectedItem, items.size())
				: DecrementWrapAround(selectedItem, items.size());
				
			return true;
	
		} else if (event.keypress.vkc == VK_RETURN) {
			ActionGotoItem(this, selectedItem, (event.keypress.mods & KM_Ctrl) != 0);
			return true;
		}
	}
	
	return false;	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorDiagnosticsList::IsDiagnosticsList() const {
	return true;
}