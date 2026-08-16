#include "editor-search.hh"
#include "basic.hh"
#include "globals.hh"
#include "events.hh"
#include "main-window.hh"
#include "settings.hh"

#include "util/rect-util.hh"
#include "logging.hh"

#include "graphics/effects.hh"
#include "graphics/glyph-run.hh"
#include "editor/editor.hh"
#include "ui/constants.h"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d2d1_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define THREAD_EXIT_CANCLED  0
#define THREAD_EXIT_FINISHED 1

static DWORD WINAPI WorkerThread(LPVOID userdata);
static bool StartNewSearch(EditorSearch* self, std::string_view searchTerm);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorSearch* EditorSearch::Make(Editor* editor, bool showReplace) {
	
	auto self = std::make_unique<EditorSearch>();
	
	ASSERT(editor)
	self->owner = editor;

	// shape headline
	if (!self->glyphRunHeadline.Shape("Find & Replace", settings.fontUi)) {
		LogError("failed to shape headline");
		return nullptr;
	}

	// text boxes
	{
		std::string initText = {};
		if (editor->textController.HasSelection()) {

			TextPosition from, to;
			editor->textController.GetSelection(&from, &to);

			// make sure we only get one line
			if (from.line != to.line) {
				to = TextPosition {
					.line = from.line,
					.column = editor->GetBuffer().GetLineAt(from.line).length };
			}

			initText = editor->GetBuffer().GetText(from, to);
			StartNewSearch(self.get(), initText);
		}

		if (!self->textboxSearch.Init(&settings.fontEditor, "Search", std::move(initText))) {
			LogError("init search textbox failed");
			return nullptr;
		}

		if (!self->textboxReplace.Init(&settings.fontEditor, "Replace")) {
			LogError("init replace textbox failed");
			return nullptr;
		}
	}

	self->focusedTextbox = (showReplace ? &self->textboxReplace : &self->textboxSearch);
	self->isReplaceTextboxVisible = showReplace;
	self->textboxSearch.inactive  = showReplace;
	self->textboxReplace.inactive = !showReplace;
	
	//self->OnResize();
	return self.release();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void CancelSearch(EditorSearch* self) {

	if (self->hThread) {

		ASSERT(self->threadData);
		self->threadData->isCanceled = true;

		const DWORD res = WaitForSingleObject(self->hThread, 1500);
		if (res != WAIT_OBJECT_0) {
			LogWarning("cancling thread failed. Result: %s", StrWaitRes(res));
		}

		CloseHandle(self->hThread);
		delete self->threadData;

		self->hThread = NULL;
		self->threadData = nullptr;
	}

}

static bool StartNewSearch(EditorSearch *self, std::string_view searchTerm) {

	ASSERT(!self->threadData);
	ASSERT(!self->hThread);

	self->threadData = new EditorSearch::ThreadData {
		.self = self,
		.textBuffer = &self->owner->GetBuffer(),
		.searchTerm = std::string {searchTerm},
		.results = {},
		.isCanceled = false,
		.isComplete = false };
	
	self->hThread = CreateThread(NULL, 0, WorkerThread, self->threadData, 0, nullptr);
	if (self->hThread == NULL) {
		LogError("CreateThread() failed. Last Error: %s", StrLastErr(GetLastError()));

		delete self->threadData;
		self->threadData = nullptr;

		return false;
	}

	return true;
}

bool EditorSearch::IsSearchComplete() const {
	return (threadData && threadData->isComplete);
}

void EditorSearch::ToggleReplaceTextbox(bool show) {
		
	if (show) {
		isReplaceTextboxVisible = true;
		focusedTextbox = &textboxReplace;
	} else {
		isReplaceTextboxVisible = false;
		focusedTextbox = &textboxSearch;
	}
		
	textboxSearch.inactive  = (focusedTextbox == &textboxReplace);
	textboxReplace.inactive = (focusedTextbox == &textboxSearch);
	//OnResize();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool EditorSearch::IsSearch() const {
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static D2D_RECT_F GetResultListButtonArea(const EditorSearch* self) {
	return D2D_RECT_F {
		.left   = self->area.left,
		.top    = self->area.bottom - settings.fontUi.lineHeight - PADDING_X2,
		.right  = self->area.right,
		.bottom = self->area.bottom };
}

static D2D_RECT_F GetSearchModifierButtonArea(const EditorSearch* self, int buttonIndex) {
	return D2D_RECT_F {
		.left   = self->area.right  - (MARGIN * (buttonIndex+1)) - (settings.fontUi.lineHeight * (buttonIndex+1)),
		.top    = self->area.top    +  MARGIN,
		.right  = self->area.right  - (MARGIN * (buttonIndex+1)) - (settings.fontUi.lineHeight * (buttonIndex)),
		.bottom = self->area.top    +  MARGIN + settings.fontUi.lineHeight};
}

static void OnClickToggleResultList(void* ud, u64) {
	auto self = static_cast<EditorSearch*>(ud);
	self->isResultListVisible = !self->isResultListVisible;
}

static void OnClickResultItem(void* ud, u64 i) {
	auto self = static_cast<EditorSearch*>(ud);
	
	ASSERT(self->threadData && self->threadData->isComplete);
	
	const EditorSearch::SearchResult& result = self->threadData->results[i];
	self->owner->ScrollToLine(result.from.line);
	self->owner->textController.SetSelection(result.from, result.to);
}

void EditorSearch::OnUpdate() {

	D2D_RECT_F area {};

	//
	// calc size
	//
	{
		const float headlineHeight = settings.fontUi.lineHeight;
		
		const float totalWidth = (RectWidth(owner->area) * 0.3f);
		float totalHeight = MARGIN + headlineHeight + MARGIN + textboxSearch.Height() + MARGIN;
		
		if (isReplaceTextboxVisible)
			totalHeight += textboxReplace.Height() + PADDING;
			
		if (threadData)
			totalHeight += settings.fontUi.lineHeight + PADDING_X2;
		
		area = MakeRect(
			owner->area.right - SCROLLBAR_WIDTH_WIDE - totalWidth - MARGIN,
			owner->area.top + MARGIN,
			totalWidth,
			totalHeight);
		
		textboxSearch.position = D2D_POINT_2F {
			.x = area.left + MARGIN,
			.y = area.top  + MARGIN_X2 + headlineHeight };
		textboxSearch.width = totalWidth - MARGIN_X2;
		
		textboxReplace.position = D2D_POINT_2F {
			.x = textboxSearch.position.x,
			.y = textboxSearch.position.y + textboxSearch.Height() + PADDING };
		textboxReplace.width = textboxSearch.width;
	}

	//
	// draw background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, area);
		if (!background) return;
		DEFER(background->Release());
	
		DrawGlow(deviceContext, 	background, area);
			
		PushLayer(deviceContext, area);
		BlurArea(deviceContext, area, background);
		PopLayer(deviceContext);
	}

	//
	// draw headline
	//
	{
		glyphRunHeadline.Draw(deviceContext,
			area.left + MARGIN,
			area.top  + MARGIN,
			settings.fontUi,
			settings.GetBrushUiText());
			
		deviceContext->DrawLine(
			D2D1_POINT_2F {
				.x = area.left + MARGIN,
				.y = area.top  + MARGIN + settings.fontUi.lineHeight },
			D2D1_POINT_2F {
				.x = area.left + MARGIN + glyphRunHeadline.width,
				.y = area.top  + MARGIN + settings.fontUi.lineHeight },
			settings.GetBrushUiText());
	}

	textboxSearch.OnUpdate();
		
	if (isReplaceTextboxVisible)
		textboxReplace.OnUpdate();	

	if (threadData) {

		//
		// draw result list button
		//
		{
			deviceContext->DrawBitmap(
				isResultListVisible
					? settings.icons.editorSearchResultsOpened
					: settings.icons.editorSearchResultsClosed,
				D2D_RECT_F {
					.left   = area.left   + PADDING_X2,
					.top    = area.bottom - PADDING    - settings.fontUi.lineHeight,
					.right  = area.left   + PADDING_X2 + settings.fontUi.lineHeight,
					.bottom = area.bottom - PADDING });

			char textBuffer[32] {0};
			if (!threadData->isComplete) {
				strcpy_s(textBuffer, "searching...");
			} else {
				sprintf_s(textBuffer, "%zu results", threadData->results.size());
			}
			
			staticGlyphRun.ShapeAndDraw(deviceContext,
				std::string_view {textBuffer},
				area.left   + settings.fontUi.lineHeight + PADDING_X2 + settings.fontUi.GetSpaceAdvance(),
				area.bottom - settings.fontUi.lineHeight - PADDING,
				settings.fontUi,
				settings.GetBrushUiText());
			
			const D2D_RECT_F resultListButtonArea {
				.left   = area.left,
				.top    = area.bottom - settings.fontUi.lineHeight - PADDING_X2,
				.right  = area.right,
				.bottom = area.bottom};
			
			if (mouse.Hittest(resultListButtonArea, this, OnClickToggleResultList))
				deviceContext->FillRoundedRectangle(MakeRoundedRect(resultListButtonArea, RADIUS), settings.GetBrushHover(mouse.isDown));
		}

		//
		// draw results
		//
		if (isResultListVisible) {

			u64 maxLocationLength = 0;
			
			// draw line number
			for (usize i = 0; i < threadData->results.size(); i++) {
				
				const SearchResult &result = threadData->results[i];
				
				char locationBuffer[32] {0};
				const u64 locationBufferLen = sprintf_s(locationBuffer, "%zu:%zu", result.from.line, result.from.column);

				if (maxLocationLength < locationBufferLen)
					maxLocationLength = locationBufferLen;
				
				staticGlyphRun.Shape(locationBuffer, settings.fontEditor);
				staticGlyphRun.Draw(deviceContext,
					area.left,
					area.bottom + (i * settings.fontEditor.lineHeight),
					settings.fontEditor,
					settings.GetBrushUiBackground());
			}
			
			// draw actual line
			for (usize i = 0; i < threadData->results.size(); i++) {
				
				const SearchResult& result = threadData->results[i];

				const TextBuffer::Line& line = owner->GetBuffer().GetLineAt(result.from.line);
				std::string_view lineText = line.GetText();

				usize numSkippedCharacters = 0u;
				if (const usize idx = lineText.find_first_not_of(" \t\v"); idx != std::string_view::npos) {
					lineText = lineText.substr(idx);
					numSkippedCharacters = idx;
				}
				
				for (const TextController::Caret& caret : owner->textController.carets) {
					if (caret.position >= result.from && caret.position <= result.to) {
						deviceContext->DrawRectangle(
							D2D1_RECT_F {
								.left   = area.left,
								.top    = area.bottom + (settings.fontEditor.lineHeight * i),
								.right  = area.right,
								.bottom = area.bottom + (settings.fontEditor.lineHeight * (i+1))},
							settings.GetBrushUiText(false));
						break;
					}
				}
				
				staticGlyphRun.Shape(lineText, settings.fontEditor);

				const float locationOffset = settings.fontEditor.GetSpaceAdvance() * maxLocationLength;
				deviceContext->FillRectangle(
					D2D1_RECT_F {
						.left   = area.left   + locationOffset + staticGlyphRun.MeasureOffset(result.from.column - numSkippedCharacters),
						.top    = area.bottom + (settings.fontEditor.lineHeight * i),
						.right  = area.left   + locationOffset + staticGlyphRun.MeasureOffset(result.to.column - numSkippedCharacters),
						.bottom = area.bottom + (settings.fontEditor.lineHeight * (i+1)) },
					settings.GetBrushUiSearchResult());

				staticGlyphRun.Draw(deviceContext,
					area.left + locationOffset,
					area.bottom + (i * settings.fontEditor.lineHeight),
					settings.fontEditor,
					settings.GetBrushUiText());
				
				const D2D1_RECT_F itemRect {
					.left   = area.left,
					.top    = area.bottom + (settings.fontEditor.lineHeight * i),
					.right  = area.right,
					.bottom = area.bottom + (settings.fontEditor.lineHeight * (i+1))};
				
				if (mouse.Hittest(itemRect, this, OnClickResultItem, i)) {
					deviceContext->FillRectangle(
						D2D1_RECT_F {
							.left   = area.left,
							.top    = area.bottom + (settings.fontEditor.lineHeight * i),
							.right  = area.right,
							.bottom = area.bottom + (settings.fontEditor.lineHeight * (i+1))},
						settings.GetBrushHover());
				}
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void ActionGotoNextSearchResult(EditorSearch* self, bool prev) {
	
	if (self->threadData->results.empty()) return;
	
	const EditorSearch::SearchResult* newResult = nullptr;
	if (prev) {
		for (s64 i = static_cast<s64>(self->threadData->results.size() - 1u); i >= 0u; i--) {
			const EditorSearch::SearchResult& result = self->threadData->results[i];
			
			if (self->owner->textController.carets.front().position > result.to) {
				newResult = &result;
				break;
			}
		}
		
		if (!newResult)
		 	 newResult = &self->threadData->results.back();
		
	} else {
		for (u64 i = 0u; i < self->threadData->results.size(); i++) {
			const EditorSearch::SearchResult& result = self->threadData->results[i];
			
			if (self->owner->textController.carets.front().position < result.from) {
				newResult = &result;
				break;
			}
		}
		
		if (!newResult)
		 	 newResult = &self->threadData->results.front();
	}

	self->owner->ScrollToLine(newResult->from.line);
	self->owner->textController.SetSelection(newResult->from, newResult->to);	
}

static void ActionReplaceNext(EditorSearch* self, bool prev) {
	ASSERT(self->threadData);
	
	if (self->threadData->results.empty())
		return;
	
	//
	// check if the current selection corresponds to a search result
	//
	std::vector<EditorSearch::SearchResult>::iterator itSearchResult;
	{
		TextPosition selectionFrom, selectionTo;
		const bool hasSelection = self->owner->textController.GetSelection(&selectionFrom, &selectionTo);

		if (hasSelection && selectionFrom.line == selectionTo.line) {
			for (auto it = self->threadData->results.begin(); it != self->threadData->results.end(); ++it) {
				if (it->from == selectionFrom && it->to == selectionTo) {
					itSearchResult = it;
					break;
				}
			}
		}
	}
	
	//
	// we found a fitting search result
	//
	if (itSearchResult != self->threadData->results.end()) {
		
		//
		// do the repalcement
		//
		const std::string_view replacementText = self->textboxReplace.GetText();
		
		// NOTE:
		// we currently use the xxxInLine()-functions because multi line search/repalce
		// is currently not supported. If this ever changes, we need to use the
		// standard remove/insert functions
		
		TextChange* change = self->owner->textController.NewTextChange();
		
		TextBuffer& textBuffer = self->owner->GetBuffer();
		
		TextChangeOperation* replaceOperation = change->NewOperation();		
		textBuffer.RemoveInLine(itSearchResult->from.line, itSearchResult->from.column, itSearchResult->to.column, replaceOperation);
		textBuffer.InsertInLine(itSearchResult->from, replacementText, replaceOperation);
		
		self->owner->ProcessTextChange(change);
		self->owner->PrepareInsertAnimation();
		self->owner->AddInsertAnimationData(replaceOperation->start, replaceOperation->insertionEnd);
		self->owner->StartInsertAnimation();
		
		//
		// remove the entry from the search result list
		//
		auto itNextResult = self->threadData->results.erase(itSearchResult);
		
		if (!self->threadData->results.empty()) {
			
			// adjust the position of any result that occurs on the same line
			for (auto it = itNextResult; it != self->threadData->results.end(); ++it) {
				ASSERT(it->from.line == it->to.line);
				
				if (it->from.line != replaceOperation->start.line) break;
				
				it->from.column -= replaceOperation->removalEnd.column - replaceOperation->start.column;
				it->from.column += replaceOperation->insertionEnd.column - replaceOperation->start.column;	
			}
			
			if (itNextResult == self->threadData->results.end())
				itNextResult  = self->threadData->results.begin();
		
			self->owner->textController.SetSelection(itNextResult->from, itNextResult->to);
			self->owner->ScrollToLine(itNextResult->from.line);
		
		// no more results are left...
		} else {
			self->owner->textController.SetCaretPosition(replaceOperation->start);
		}

	//
	// we are not at one of the results - move to closest result 
	//
	} else {
		ActionGotoNextSearchResult(self, prev);
	}
}

static void ActionReplaceAll(EditorSearch* self) {
	ASSERT(self->threadData);
	
	if (self->threadData->results.empty())
		return;

	// @TODO do we need a message box here that warns the we are about to replace X results?
	
	const std::string_view replacementText = self->textboxReplace.GetText();
	TextBuffer& textBuffer = self->owner->GetBuffer();
	
	TextChange* change = self->owner->textController.NewTextChange();
	change->ReserveCapacity(self->threadData->results.size());

	self->owner->PrepareInsertAnimation(self->threadData->results.size());
	
	for (u64 i = 0u; i < self->threadData->results.size(); i++) {
		const EditorSearch::SearchResult& result = self->threadData->results[i];
	
		TextChangeOperation* replaceOperation = change->NewOperation();
		textBuffer.RemoveInLine(result.from.line, result.from.column, result.to.column, replaceOperation);
		textBuffer.InsertInLine(result.from, replacementText, replaceOperation);
	
		// adjust the position of any result that occurs on the same line	
		for (u64 j = i + 1; j < self->threadData->results.size(); j++) {
			EditorSearch::SearchResult& nextResult = self->threadData->results[j];
			ASSERT(nextResult.from.line == nextResult.to.line);
			
			if (nextResult.from.line != replaceOperation->start.line) break;
			
			nextResult.from.column -= replaceOperation->removalEnd.column - replaceOperation->start.column;
			nextResult.from.column += replaceOperation->insertionEnd.column - replaceOperation->start.column;	
		}
		
		self->owner->AddInsertAnimationData(replaceOperation->start, replaceOperation->insertionEnd);
	}
	
	self->threadData->results.clear();
	self->owner->ProcessTextChange(change);
	self->owner->StartInsertAnimation();
}

static void ActionSetCaretToEveryResult(EditorSearch* self, bool select) {
	ASSERT(self->threadData);
	
	if (self->threadData->results.empty())
		return;
	
	if (self->threadData->results.size() == 1) {
		const EditorSearch::SearchResult& result = self->threadData->results.front();
		if (select) self->owner->textController.SetSelection(result.from, result.to);
		else        self->owner->textController.SetCaretPosition(result.from);
		return;
	}
	
	self->owner->textController.carets.clear();
	self->owner->textController.carets.reserve(self->threadData->results.size());
	for (const EditorSearch::SearchResult& result : self->threadData->results) {
		self->owner->textController.carets.push_back(TextController::Caret {
			.position = result.from,
			.selection = select ? result.to : TextPosition {},
			.hasSelection = select});
	}
}

bool EditorSearch::OnKeyEvent(KeyEvent event, Command command) {
	
	if (event.vkeycode == VK_RETURN) {
	
		if (!threadData || searchIsDirty) {

			CancelSearch(this);

			const std::string_view searchTerm = textboxSearch.GetText();
			StartNewSearch(this, searchTerm);

			searchIsDirty = false;

		} else if (IsSearchComplete()) {
				const bool ctrl = (event.modifiers & KM_Ctrl) != 0;
				const bool shift = (event.modifiers & KM_Shift) != 0;
				const bool alt = (event.modifiers & KM_Alt) != 0;
 			
 			if (!ctrl && !alt) {
				if (focusedTextbox == &textboxReplace)
					ActionReplaceNext(this, shift);
				else if (focusedTextbox == &textboxSearch)
					ActionGotoNextSearchResult(this, shift);
				else
					ASSERT_UNREACHABLE;
			
			} else if (ctrl && !alt) {
			
				if (focusedTextbox == &textboxReplace)
					ActionGotoNextSearchResult(this, shift);
				else if (isReplaceTextboxVisible)
					ActionReplaceNext(this, shift);
			
			} else if (ctrl && alt) {
				ActionSetCaretToEveryResult(this, shift);	
			
			} else if (ctrl && !alt) {
				ActionReplaceAll(this);	
			}
		}

	} else if (event.vkeycode == VK_PAUSE && (event.modifiers & KM_Ctrl) != 0) {
		CancelSearch(this);

	} else if (event.vkeycode == VK_TAB && (event.modifiers & KM_Ctrl) != 0) {

		if (isReplaceTextboxVisible) {
			textboxSearch.inactive  = !textboxSearch.inactive;
			textboxReplace.inactive = !textboxReplace.inactive;
			focusedTextbox = (focusedTextbox == &textboxSearch)
				? &textboxReplace
				: &textboxSearch;

			ASSERT(textboxSearch.inactive ^ textboxReplace.inactive);
		}

	} else if (command.id == Command::Id_Editor_OpenSearch) {
		if (command.parameters->boolValue) {
			isReplaceTextboxVisible = true;
			textboxSearch.inactive = true;
			textboxReplace.inactive = false;
			focusedTextbox = &textboxReplace;
		} else {
			isReplaceTextboxVisible = false;
			textboxSearch.inactive = false;
			textboxReplace.inactive = true;
			focusedTextbox = &textboxSearch;
		}
		
	} else {
		ASSERT(focusedTextbox);
		const bool changed = focusedTextbox->OnKeyDown(event, command);
		searchIsDirty |= changed && (focusedTextbox == &textboxSearch);
	}
	
	return true;
}

void EditorSearch::OnChar(const char* data, u64 len) {
	ASSERT(focusedTextbox);
	const bool changed = focusedTextbox->OnChar(data, len);
	searchIsDirty |= changed && (focusedTextbox == &textboxSearch);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static DWORD WINAPI WorkerThread(LPVOID userdata) {
	
	auto td  = static_cast<EditorSearch::ThreadData*>(userdata);

	if (td->searchTerm.empty()) {
		td->isComplete = true;
		return THREAD_EXIT_FINISHED;
	}
	
	for (usize i = 0; i < td->textBuffer->LineCount(); i++) {

		const TextBuffer::Line &line = td->textBuffer->GetLineAt(i);
		std::string_view textRemaining = line.GetText();
		
		while (!textRemaining.empty()) {

			const auto it = std::search(
				textRemaining.begin(), textRemaining.end(),
				td->searchTerm.begin(), td->searchTerm.end(),
				[] (char lhs, char rhs) { return std::tolower(lhs) == std::tolower(rhs); });
				
			if (it != textRemaining.end()) {
				const usize pos = &*it - line.data;
				
				td->results.push_back(EditorSearch::SearchResult {
					.from = TextPosition {
						.line = i,
						.column = pos },
					.to = TextPosition {
						.line = i,
						.column = pos + td->searchTerm.size() }});

				textRemaining = std::string_view {it + td->searchTerm.size(), textRemaining.end()};
			
			}
			else {
				// NOTE do not just break here; we would never check for the cancelation keep searching every line
				textRemaining = std::string_view {};
			}			

			if (td->isCanceled)
				return THREAD_EXIT_CANCLED;
		}
	}

	td->isComplete = true;
	mainWindow.PostUpdate();
	return THREAD_EXIT_FINISHED;	
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorSearch::~EditorSearch() noexcept {
	CancelSearch(this);
}