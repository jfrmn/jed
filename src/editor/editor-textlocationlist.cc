#include "editor-textlocationlist.hh"
#include "events.hh"
#include "globals.hh"
#include "editor.hh"
#include "settings.hh"
#include "events.hh"
#include "app.hh"

#include "ui/constants.h"

#include "glyph-run.hh"
#include "graphics.hh"

#include "logging.hh"
#include "util.hh"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

EditorTextLocationList* EditorTextLocationList::Make(Editor* owner) {
	auto self = new EditorTextLocationList();
	self->owner = owner;
	self->references = 2;
	self->filePreview.Init();
	return self;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void EditorTextLocationList::Update() {
	if (DrawIncompleteState(deviceContext, "No locations found.")) return;
	
	auto glyphRuns = new std::pair<GlyphRun, GlyphRun>[itemCount];
	DEFER(delete[] glyphRuns);
	
	char currentPathBuffer[MAX_PATH + 1];
	const u64 currentPathLength = GetCurrentDirectory(MAX_PATH, currentPathBuffer);
	if (currentPathLength == 0u) {
		LogError("GetCurrentDirectory() failed. Last Error: %s", StrLastErr(GetLastError()));
		return;
	}
	
	const std::string_view currentPath {currentPathBuffer, currentPathLength};
	
	//
	// shape text
	//
	f32 width = 0.0f;
	for (u64 i = 0u; i < itemCount; i++) {
		GlyphRun& runLabel    = glyphRuns[i].first;
		GlyphRun& runFullPath = glyphRuns[i].second;
		Item& item = items[i];
		
		const std::string label = item.selectionRange.start.line == item.selectionRange.end.line
			? FormatString("%.*s:%zu",      SIZE_AND_DATA(item.filename), item.selectionRange.start.line)
			: FormatString("%.*s:%zu-%zu", SIZE_AND_DATA(item.filename), item.selectionRange.start.line, item.selectionRange.end.line);
		
		const std::string_view directory = GetDirectoryFromPath(item.targetPath);
		
		runLabel.Shape(label, settings.fontUi);
		runFullPath.Shape(directory, settings.fontUi);
		
		const f32 currentItemWidth = PADDING_X3 + runLabel.width + runFullPath.width;
		if (width < currentItemWidth)
			width = currentItemWidth;
	}
	
	const f32 height = settings.fontUi.lineHeight * itemCount;
	const D2D_POINT_2F position = GetPosition();	
	
	//
	// blur background
	//
	BlurArea(deviceContext, MakeRect(position.x, position.y, width, height));
	
	//
	// draw items
	//
	for (u64 i = 0u; i < itemCount; i++) {
		GlyphRun& runLabel    = glyphRuns[i].first;
		GlyphRun& runFullPath = glyphRuns[i].second;
		
		const f32 posY = position.y + (settings.fontUi.lineHeight * i);
		
		if (i == selectedItem) {
			deviceContext->FillRectangle(
				D2D1_RECT_F {
					.left   = position.x,
					.top    = posY,
					.right  = position.x + width, //runLabel.GetTotalAdvance() + runFullPath.GetTotalAdvance() + PADDING_X3,
					.bottom = position.y + settings.fontUi.lineHeight},
				settings.GetBrushSelection());
		}
		
		runLabel.Draw(deviceContext, position.x + PADDING, posY, settings.fontUi, settings.GetBrushUiText());
		runFullPath.Draw(deviceContext, position.x + PADDING_X2 + runLabel.width, position.y + (settings.fontUi.lineHeight * i), settings.fontUi, settings.GetBrushUiText(false));
	}
	
	//
	// draw preview
	//
	{
		filePreview.x = position.x + width;
		filePreview.y = position.y + (selectedItem * settings.fontUi.lineHeight);
		filePreview.OnUpdate();
		deviceContext->DrawRectangle(filePreview.GetArea(), settings.GetBrushSelection());	
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void EditorTextLocationList::UpdateFilePreview() {
	const Item& item = items[selectedItem];
		
	FilePreview::LoadArgs args {
		.path = item.targetPath,
		.hasSelection = true,
		.selectionFrom = item.selectionRange.start,
		.selectionTo = item.selectionRange.end};
	
	if (extendedItems) {
		const ItemEx& itemEx = static_cast<const ItemEx&>(item);
		args.mode = FilePreview::LoadMode_LineRange;
		args.lineFrom = itemEx.fullTargetRange.start.line;
		args.lineTo = itemEx.fullTargetRange.end.line;
	} else {
		args.mode = FilePreview::LoadMode_TargetLine;
		args.lineFrom = item.selectionRange.start.line;
	}
	
	if (!filePreview.Load(args))
		LogWarning("init file preview failed");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorTextLocationList::HandleEvent(const Event& event) {
	
	if (event.type == Event::Type_KeyPress && event.keypress.vkc == VK_RETURN) {
		if (selectedItem < U64_MAX) {
			ASSERT(state == State_Completed);
			ASSERT(selectedItem < itemCount)
			
			// NOTE: OpenEditor (with openBehv = UpdateCurrent) might lead to this being deleted
			// we we need to set this to null here
			owner->editorCaretAttached = nullptr;
			DEFER(RemoveReference());
			
			const Item& item = items[selectedItem];
			
			bool wasOpen = false;
			const auto openBehav = OpenBehaviorFromModifiers(event.keypress.mods);
			Editor* openedEditor = app.OpenEditor(item.targetPath, openBehav, &wasOpen);
			if (!openedEditor) {
				error = "Failed to open file";
				state = State_Errored;
				return true;
			}
			
			openedEditor->textController.SetSelection(item.selectionRange.start, item.selectionRange.end);
			
			if (wasOpen) {
				openedEditor->ScrollToLine(item.selectionRange.start.line);
			
			} else {
				// The scrollarea.totalSize is not set yet. Right now, it gets updated every frame but no frame has been rendered yet
				// Therefore scrollarea.GetMaxPositionY() does not work correctly and we need to calc the max. position manually here
				const f32 maxPosition = (openedEditor->textController.buffer.LineCount() * settings.fontEditor.lineHeight) - openedEditor->scrollarea.vpSize.height;
				const f32 targetPostion = (settings.fontEditor.lineHeight * item.selectionRange.start.line) - (openedEditor->scrollarea.vpSize.height * 0.4f);
				
				openedEditor->scrollarea.vpY = std::clamp(targetPostion, 0.0f, maxPosition);
			}
		}
		
		return true;
				
	} else if (event.type == Event::Type_KeyPress && (event.keypress.vkc == VK_UP || event.keypress.vkc == VK_DOWN) && event.keypress.mods == KM_None) {
		selectedItem = (event.keypress.vkc == VK_UP)
			? IncrementWrapAround(selectedItem, itemCount)
			: DecrementWrapAround(selectedItem, itemCount);
		
		UpdateFilePreview();
	}
	
	return false;
}

void EditorTextLocationList::OnInput() {
	owner->editorCaretAttached = nullptr;
	RemoveReference();
}

EditorTextLocationList::~EditorTextLocationList() {
	delete[] items;
}