#include "editor-autocomplete.hh"
#include "editor/editor.hh"
#include "editor/editor-signaturehelp.hh"
#include "globals.hh"
#include "events.hh"
#include "settings.hh"

#include "ui/constants.h"
#include "util/rect-util.hh"
#include "text/text-position.hh"
#include "graphics/effects.hh"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>


EditorAutocomplete* EditorAutocomplete::Make(Editor* editor) {

	auto self = std::make_unique<EditorAutocomplete>();
	self->owner = editor;	
	self->triggerPosition = editor->textController.carets.front().position;
		
	return self.release();
}

EditorAutocomplete::~EditorAutocomplete() noexcept {
	delete[] items;	
		
	if (signatureHelp)
		signatureHelp->RemoveReference();
}

void EditorAutocomplete::OnUpdate() {
	if (DrawIncompleteState(deviceContext, "No suggestions.")) return;
	
	ASSERT(itemCount > 0);
	ASSERT(items);
	
	const D2D1_POINT_2F position = GetPosition();
	
	auto glyphRuns = new std::pair<GlyphRun, GlyphRun>[itemCount];
	DEFER(delete[] glyphRuns);
	
	f32 width = .0f;
	f32 height = itemCount * settings.fontEditor.lineHeight;
	{
		for (u64 i = 0u; i < itemCount; i++) {
			GlyphRun& runLabel    = glyphRuns[i].first;
			GlyphRun& runDetails  = glyphRuns[i].second;
			
			runLabel.Shape(items[i].label, settings.fontEditor);
			runDetails.Shape(items[i].details, settings.fontEditor);
			
			const f32 currentWidth = (PADDING_X2 + settings.fontEditor.GetSpaceAdvance()) + runLabel.width + runDetails.width + PADDING_X3;
			if (width < currentWidth)
				width = currentWidth;
		}
	}
	
	//
	// draw background
	//
	{	
		const D2D_RECT_F area = MakeRect(position.x, position.y, width, height);
		BlurArea(deviceContext, area);
	}
		
	//
	// draw items
	//
	for (u64 i = 0u; i < itemCount; i++) {
		const Item& item = items[i];
		const GlyphRun& runLabel = glyphRuns[i].first;
		const GlyphRun& runDetails  = glyphRuns[i].second;
		
		const f32 offsety = (settings.fontEditor.lineHeight * i);
		
		if (i == selectedItem) {
			deviceContext->FillRectangle(
				MakeRect(position.x, position.y + offsety, width, settings.fontEditor.lineHeight),
				settings.GetBrushSelection());
		}
		
		// draw iocn
		ID2D1Bitmap* icon = item.type == Item::Type_Unknown
			? settings.icons.unknown
			: *(&settings.icons.editorAutocompleteText + item.type - 1);
		
		deviceContext->DrawBitmap(icon,
			D2D1_RECT_F {
				.left   = position.x + PADDING,
				.top    = position.y + offsety,
				.right  = position.x + PADDING + settings.fontEditor.lineHeight,
				.bottom = position.y + offsety + settings.fontEditor.lineHeight});		
		
		const f32 textPosX = position.x + PADDING_X2 + settings.fontEditor.lineHeight;
		const f32 textPosY = position.y + offsety;
		
		// draw match result
		if (item.matched) {
			f32 offsetFrom, offsetTo;
			runLabel.MeasureOffsetRange(
				item.matchResult.position,
				item.matchResult.position + item.matchResult.length,
				&offsetFrom, &offsetTo);
			
			deviceContext->FillRectangle(
				D2D1_RECT_F {
					.left   = textPosX + offsetFrom,
				    .top    = textPosY,
				    .right  = textPosX + offsetTo,
				    .bottom = textPosY + settings.fontUi.lineHeight},
				settings.GetBrushUiSearchResult());
		}
		
		runLabel.Draw(deviceContext,
			textPosX,
			textPosY,
			settings.fontEditor,
			settings.GetBrushUiText());
		
		runDetails.Draw(deviceContext,
			textPosX + PADDING + runLabel.width,
			textPosY,
			settings.fontEditor,
			settings.GetBrushUiText(false));
	}
	
	// draw signature help if active
	if (signatureHelp)
		signatureHelp->OnUpdate();
}

static void InsertItem(EditorAutocomplete* self) {
	if (self->state != EditorCaretAttached::State_Completed) return;
	
	// NOTE: it's important that we set this to null before we apply the edits
	// because otherwise the edits would be passed to us again in ProcessTextChange()	
	self->owner->editorCaretAttached = nullptr;
	
	if (self->selectedItem == U64_MAX) return;
	
	const EditorAutocomplete::Item& item = self->items[self->selectedItem];

	TextChange* textChange = self->owner->textController.NewTextChange();
	TextChangeOperation* operation = textChange->NewOperation();	

	if (item.insertPosition.column != self->owner->textController.carets.front().position.column) {
		self->owner->GetBuffer().RemoveInLine(
			item.insertPosition.line,
			item.insertPosition.column,
			self->owner->textController.carets.front().position.column,
			operation);
	}
		
	self->owner->GetBuffer().InsertInLine(item.insertPosition, item.insertText, operation);
	self->owner->textController.SetCaretPosition(operation->insertionEnd);

	self->owner->ProcessTextChange(textChange);
		
	ASSERT(item.insertPosition.line == self->owner->textController.carets.front().position.line);
	self->owner->PrepareInsertAnimation();
	self->owner->AddInsertAnimationData(operation->start, operation->insertionEnd);
	self->owner->StartInsertAnimation();
}

void EditorAutocomplete::SortItems() {
	
	for (u64 i = 0u; i < itemCount; i++) {
		Item &item = items[i];

		const TextBuffer::Line& line = owner->GetBuffer().GetLineAt(item.insertPosition.line);
		
		const u64 textBeforeCursorLen = (owner->textController.carets.front().position.column - item.insertPosition.column);
		const std::string_view textBeforeCursor = line.GetText().substr(item.insertPosition.column, textBeforeCursorLen);
		
		if (textBeforeCursor.empty())
			continue;
		
		FuzzyMatchResult matchResult {};
		if (FuzzyMatch(textBeforeCursor, item.label, &matchResult)) {
			item.matched = true;
			item.matchResult = matchResult;
			
		} else {
			item.matched = false;
		}
	}
	
	std::sort(items, items + itemCount, [] (const Item&	lhs, const Item& rhs) {
		if (lhs.matched && !rhs.matched) return true;
		if (!lhs.matched && rhs.matched) return false;
		
		if (const int cmp = CompareFuzzyMatchResults(lhs.matchResult, rhs.matchResult); cmp != 0)
			return cmp > 0;
			
		return lhs.label < rhs.label;
	});
}

bool EditorAutocomplete::OnKeyEvent(KeyEvent event) {
	if (!items || itemCount == 0u) return false;

	if ((event.vkeycode == VK_DOWN || event.vkeycode == VK_UP) && event.modifiers == KM_None) {
		
		selectedItem = event.vkeycode == VK_DOWN ?
			IncrementWrapAround(selectedItem, itemCount):
			DecrementWrapAround(selectedItem, itemCount);

		return true;
	
	} else if ((event.vkeycode == VK_RETURN || event.vkeycode == VK_TAB) && event.modifiers == KM_None) {
		InsertItem(this);
		RemoveReference();
		return true;
	
	} else if (signatureHelp) {
		return signatureHelp->OnKeyEvent(event);
	}
	
	return false;
}

void EditorAutocomplete::OnInput() {
	
	const TextPosition cursor = owner->textController.carets.front().position;
	if (cursor.line != triggerPosition.line || cursor.column < triggerPosition.column) {
		owner->editorCaretAttached = nullptr;
		RemoveReference();
	
	} else {
		SortItems();
	}
}
