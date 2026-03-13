#include "editor-textlocationlist.hh"
#include "events.hh"
#include "globals.hh"
#include "editor.hh"
#include "main-window.hh"
#include "theme.hh"

#include "ui/constants.h"

#include "graphics/glyph-run.hh"
#include "graphics/effects.hh"

#include "util/logging.hh"
#include "util/file-util.hh"
#include "util/rect-util.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

#include <algorithm>

EditorTextLocationList* EditorTextLocationList::Make(Editor* owner) {
	auto self = new EditorTextLocationList();
	self->owner = owner;
	self->references = 2;
	self->filePreview.Init();
	return self;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void EditorTextLocationList::OnUpdate() {
	if (DrawIncompleteState(deviceContext, "No locations found.")) return;
	
	auto glyphRuns = new std::pair<GlyphRun, GlyphRun>[itemCount];
	DEFER(delete[] glyphRuns);
	
	char currentPathBuffer[MAX_PATH + 1];
	const u64 currentPathLength = GetCurrentDirectory(MAX_PATH, currentPathBuffer);
	if (currentPathLength == 0u) {
		LogError("GetCurrentDirectory() failed. Last Error: %", FLastErr(GetLastError()));
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
			? FormatString("%:%", item.filename, item.selectionRange.start.line)
			: FormatString("%:%-%", item.filename, item.selectionRange.start.line, item.selectionRange.end.line);
		
		const std::string_view directory = GetDirectoryFromPath(item.targetPath);
		
		runLabel.Shape(label, theme.fontUi);
		runFullPath.Shape(directory, theme.fontUi);
		
		const f32 currentItemWidth = PADDING_X3 + runLabel.width + runFullPath.width;
		if (width < currentItemWidth)
			width = currentItemWidth;
	}
	
	const f32 height = theme.fontUi.lineHeight * itemCount;
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
		
		const f32 posY = position.y + (theme.fontUi.lineHeight * i);
		
		if (i == selectedItem) {
			deviceContext->FillRectangle(
				D2D1_RECT_F {
					.left   = position.x,
					.top    = posY,
					.right  = position.x + width, //runLabel.GetTotalAdvance() + runFullPath.GetTotalAdvance() + PADDING_X3,
					.bottom = position.y + theme.fontUi.lineHeight},
				theme.GetBrushSelection());
		}
		
		runLabel.Draw(deviceContext, position.x + PADDING, posY, theme.fontUi, theme.GetBrushUiText());
		runFullPath.Draw(deviceContext, position.x + PADDING_X2 + runLabel.width, position.y + (theme.fontUi.lineHeight * i), theme.fontUi, theme.GetBrushUiText(false));
	}
	
	//
	// draw preview
	//
	{
		filePreview.x = position.x + width;
		filePreview.y = position.y + (selectedItem * theme.fontUi.lineHeight);
		filePreview.OnUpdate();
		deviceContext->DrawRectangle(filePreview.GetArea(), theme.GetBrushSelection());	
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
bool EditorTextLocationList::OnKeyDown(KeyEvent event) {
	
	if (event.vkeycode == VK_RETURN) {
		if (selectedItem < U64_MAX) {
			ASSERT(state == State_Completed);
			ASSERT(selectedItem < itemCount)
			
			// NOTE: OpenEditor (with openBehv = UpdateCurrent) might lead to this being deleted
			// we we need to set this to null here
			owner->editorCaretAttached = nullptr;
			DEFER(RemoveReference());
			
			const Item& item = items[selectedItem];
			
			bool wasOpen = false;
			const auto openBehav = OpenBehaviorFromModifiers(event);
			Editor* openedEditor = mainWindow.OpenEditor(item.targetPath, openBehav, &wasOpen);
			if (!openedEditor) {
				error = "Failed to open file";
				state = State_Errored;
				return true;
			}
			
			openedEditor->textController.Select(item.selectionRange.start, item.selectionRange.end);
			
			if (wasOpen) {
				openedEditor->ScrollToLine(item.selectionRange.start.line);
			
			} else {
				// The scrollarea.totalSize is not set yet. Right now, it gets updated every frame but no frame has been rendered yet
				// Therefore scrollarea.GetMaxPositionY() does not work correctly and we need to calc the max. position manually here
				const f32 maxPosition = (openedEditor->textController.buffer.LineCount() * theme.fontEditor.lineHeight) - openedEditor->scrollarea.vpSize.height;
				const f32 targetPostion = (theme.fontEditor.lineHeight * item.selectionRange.start.line) - (openedEditor->scrollarea.vpSize.height * 0.4f);
				
				openedEditor->scrollarea.vpY = std::clamp(targetPostion, 0.0f, maxPosition);
			}
		}
		
		return true;
				
	} else if ((event.vkeycode == VK_UP || event.vkeycode == VK_DOWN) && event.NoModifiers()) {
		selectedItem = (event.vkeycode == VK_UP)
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