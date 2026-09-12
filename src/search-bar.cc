#include "search-bar.hh"
#include "basic.hh"
#include "commands.hh"
#include "tools.hh"
#include "events.hh"
#include "settings.hh"
#include "app.hh"
#include "util.hh"
#include "logging.hh"

#include "ui/constants.h"
#include "ui/window.hh"
#include "ui/parameter-configurator.hh"
#include "ui/animation.hh"
#include "graphics.hh"

#include <algorithm>

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// SearchBar
//
//////////////////////////////////////////////////////////////////////////////////////////////////

void SearchBar::Init(std::string_view placeholderText) {
	
	textBox.Init(&settings.fontUi, placeholderText);
	
	scrollarea.vpX = 0.0f;	
	scrollarea.vpY = 0.0f;
	scrollarea.barWidth = SCROLLBAR_WIDTH_NARROW;
	
	shouldClose = false;
	OnResize();
}

SearchBar::~SearchBar() noexcept {
	delete parameterConfigurator;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static float ItemHeight() {
	return MARGIN_X2 + (settings.fontUi.lineHeight * 2);
}

static void OnClickItem(void* ud, u64 i) {
	auto self = static_cast<SearchBar*>(ud);
	self->OnPickItem(i, nullptr);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::Open() {
	spawnAnimationValue = 0.0f;
	itemHighlightAnimationValue = 0.0f;
	textBox.textController.SetSelection(
		TextPosition {0, 0},
		TextPosition {0, textBox.textController.buffer.lines.front().length});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnUpdate() {

	if (parameterConfigurator) {
		parameterConfigurator->Update();
		return;
	}
		
	//
	// update animation
	//
	{
		AnimationCycling::Advance(&itemHighlightAnimationValue);
		AnimationLinear::Advance(&spawnAnimationValue, 0.008f);
	}
	
	//
	// spawn animation
	//
	const f32 halfWidth = RectWidth(area) / 2.0f;
	const f32 height = RectHeight(area);
	const D2D_RECT_F animatedArea {
		.left   = area.left + halfWidth - (halfWidth * AnimationLinear::Value(spawnAnimationValue)),
		.top    = area.top,
		.right  = area.right - halfWidth + (halfWidth * AnimationLinear::Value(spawnAnimationValue)),
		.bottom = area.top + (height * AnimationLinear::Value(spawnAnimationValue))};
	
	//
	// draw background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, animatedArea);
		if (!background) return;
		DEFER(background->Release());
		
		DrawGlow(deviceContext, background, animatedArea);
		
		PushLayer(deviceContext, animatedArea);
		BlurArea(deviceContext, animatedArea, background);
	}
		
	//
	// textbox
	//
	{
		textBox.Update();
	}
	
	//
	// draw items
	//
	{	
		const f32 itemHeight = ItemHeight();
		
		const u64 firstVisible = std::max<u64>(0u, static_cast<u64>(scrollarea.vpY / itemHeight));
		const u64 lastVisible  = std::min<u64>(itemCount, static_cast<u64>((scrollarea.vpY + scrollarea.vpSize.height) / itemHeight) + 1u);
		
		OnUpdateItems(firstVisible, lastVisible);
	}
	
	PopLayer(deviceContext);
	
	//
	// draw scrollbar
	//
	scrollarea.OnUpdate();
}

void SearchBar::UpdateItem(u64 i, const SearchBar::UpdateItemParams& params) {
	const f32 itemHeight = ItemHeight();
	const f32 itemAreaTop = area.top + MARGIN_X2 + textBox.Height();
	
	const D2D_RECT_F itemArea {
		.left   = area.left,
		.top    = itemAreaTop - scrollarea.vpY + (itemHeight * i),
		.right  = area.right,
		.bottom = itemAreaTop - scrollarea.vpY + (itemHeight * (i+1))};
		
	if (selectedItem == i) {
		ID2D1SolidColorBrush* brushGlow = settings.GetBrushDropShadow();
		const f32 opacityBefore = brushGlow->GetOpacity();
		DEFER(brushGlow->SetOpacity(opacityBefore));
		
		const f32 opacity = AnimationCycling::Value(itemHighlightAnimationValue);
		brushGlow->SetOpacity(opacity);
		
		deviceContext->FillRectangle(itemArea, brushGlow);
	}
	
	staticGlyphRun.Shape(params.text, settings.fontUi);
	
	f32 offsetFrom, offsetTo;
	staticGlyphRun.MeasureOffsetRange(
		params.matchedPosition,
	    params.matchedPosition + params.matchedLength,
	    &offsetFrom, &offsetTo);
	
	deviceContext->FillRectangle(
		D2D1_RECT_F {
			.left   = MARGIN + itemArea.left + offsetFrom,
		    .top    = MARGIN + itemArea.top,
		    .right  = MARGIN + itemArea.left + offsetTo,
		    .bottom = MARGIN + itemArea.top + settings.fontUi.lineHeight},
		settings.GetBrushUiSearchResult());

	staticGlyphRun.Draw(deviceContext,
		MARGIN + itemArea.left,
	    MARGIN + itemArea.top,
		settings.fontUi,
		settings.GetBrushUiText());
		
	staticGlyphRun.ShapeAndDraw(deviceContext, 
		params.subText,
		MARGIN + itemArea.left,
	    MARGIN + itemArea.top + settings.fontUi.lineHeight,
		settings.fontUi,
		settings.GetBrushUiText(false));
	
	if (mouse.Hittest(itemArea, this, OnClickItem))
		deviceContext->FillRectangle(itemArea, settings.GetBrushHover(mouse.isDown));
}

void SearchBar::SetItemCount(u64 newItemCount) {
	if (itemCount != newItemCount) {
		itemCount = newItemCount;
		scrollarea.totalSize.height = itemCount * ItemHeight();
		area.bottom = std::min(
			area.top + MARGIN_X2 + textBox.Height() + (itemCount * ItemHeight()),
			mainWindow.height - MARGIN);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnResize() {
 	const f32 offsetFromTop = (PADDING_X2 + settings.fontUi.lineHeight) + MARGIN;
 	const f32 itemAreaTop = offsetFromTop + MARGIN_X2 + textBox.Height();
	 	
 	area = D2D_RECT_F {
		.left   = mainWindow.width * 0.3f,
		.top    = offsetFromTop,
		.right  = mainWindow.width * 0.7f,
		.bottom = std::min(
			itemAreaTop + (itemCount * ItemHeight()),
			mainWindow.height - MARGIN)};
	
	textBox.position = D2D_POINT_2F {
		.x = area.left + MARGIN,
		.y = area.top + MARGIN},
	textBox.width = RectWidth(area) - MARGIN_X2;
	
	scrollarea.OnResize(D2D_RECT_F {
		.left = area.left,
		.top = itemAreaTop,
		.right = area.right,
		.bottom = mainWindow.height - MARGIN});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool SearchBar::HandleEvent(const Event& event) {
	if (parameterConfigurator) {
		const bool handled = parameterConfigurator->HandleEvent(event);
		if (parameterConfigurator->result != ParameterConfigurator::Result_Unfinished)
			OnFinishedParameterConfiguration();
		return handled;
	}
	
	if (event.type == Event::Type_KeyPress) {
		if ((event.keypress.vkc == VK_DOWN || event.keypress.vkc == VK_UP)) {
			if (itemCount == 0u) {
				selectedItem = U64_MAX;
			
			} else if (event.keypress.vkc == VK_DOWN) {
				selectedItem = IncrementWrapAround(selectedItem, itemCount);
	
			} else if (event.keypress.vkc == VK_UP) {
				selectedItem = DecrementWrapAround(selectedItem, itemCount);
			}
			
			itemHighlightAnimationValue = .0f;
			return true;
		
		} else if (event.keypress.vkc == VK_RETURN) {
			
			if (selectedItem < itemCount)
				OnPickItem(selectedItem, &event);
				
			return true;
			   
		} else if (event.keypress.vkc == VK_ESCAPE && event.keypress.mods == KM_None) {
			shouldClose = true;
			return true;
		}	
	}
	
	const auto [handled, changed] = textBox.HandleEvent(event);
	if (changed) FilterItems(textBox.GetText());
	return handled;
}

void SearchBar::OnMouseWheel(f32 distance) {
	if (parameterConfigurator) return;
	scrollarea.ScrollVertical(distance * settings.fontUi.lineHeight * 5);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// SearchBarFiles
//
//////////////////////////////////////////////////////////////////////////////////////////////////

static void CancelIndexing(SearchBarFiles* self) {
	if (!self->hThread) return;
	
	self->cancel = true;
	
	const DWORD res = WaitForSingleObject(self->hThread, 5000);
	if (res != WAIT_OBJECT_0) {
		LogError("failed to cancel search thread: %s", StrWaitRes(res));
		TerminateThread(self->hThread, -1);
	}
	
	CloseHandle(self->hThread);
	
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBarFiles::Init() {
	__super::Init("find file...");
}

SearchBarFiles::~SearchBarFiles() noexcept {
	CancelIndexing(this);
	
	Page* page = head;
	while (page) {
		Page* next = page->next;
		delete page;
		page = next;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static std::string_view GetFilename(const SearchBarFiles::Item& item) {
	return std::string_view {item.fullPath.data() + item.fullPath.size() - item.filenameLength, item.filenameLength}; 
}

static bool CompareItems(const SearchBarFiles::Item& litem, const SearchBarFiles::Item& ritem) {
	if (const int cmp = CompareFuzzyMatchResults(litem.fuzzyMatchResult, ritem.fuzzyMatchResult); cmp != 0)
		return cmp > 0;
					
	if (litem.filenameLength < ritem.filenameLength)
		return true;	
	if (litem.filenameLength > ritem.filenameLength)
		return false;

	const std::string_view lfilename = GetFilename(litem);
	const std::string_view rfilename = GetFilename(ritem);
	if (const int cmp = lfilename.compare(rfilename); cmp != 0)
		return cmp < 0;
						
	if (litem.fullPath.size() < ritem.fullPath.size())
		return true;
	if (litem.fullPath.size() > ritem.fullPath.size())
		return false;
		 
	return litem.fullPath < ritem.fullPath;
}

static void IndexDirectory(SearchBarFiles* self, DirectoryIterator& iterator, u64 parent, SearchBarFiles::Page*& currentPage, u64& occupied) {
		
	while (iterator.Next()) {
		
		if (self->cancel)
			return;
		
		if (occupied + iterator.filename.size() >= SearchBarFiles::Page::SIZE) {
			
			if (currentPage->next) {
				SearchBarFiles::Page* nextPage = currentPage->next;
				// NOTE: do not memset the whole page, because the page may also have next page
				// the whould get lost!
				memset(nextPage->data, 0, sizeof(nextPage->data));
				currentPage = nextPage;	
			
			} else {
				auto newPage = new SearchBarFiles::Page;
				memset(newPage, 0, sizeof(*newPage));
				currentPage->next = newPage;
				currentPage = newPage;
			}
			
			occupied = 0u;
		}
		
		memcpy_s(
			currentPage->data + occupied,
			SearchBarFiles::Page::SIZE - occupied,
			iterator.filename.data(),
			iterator.filename.size());
			
		self->index.push_back(SearchBarFiles::IndexEntry {
			.filename = std::string_view {currentPage->data + occupied, iterator.filename.size()},
			.parent = parent,
			.isDirectory = iterator.IsDirectory()});
		
		occupied += iterator.filename.size();
		
		if (iterator.IsDirectory()) {
			
			DirectoryIterator subDirIterator;
			strcpy_s(subDirIterator.searchPath, iterator.searchPath);
			char* searchPathTail = strrchr(subDirIterator.searchPath, '*');
			ASSERT(searchPathTail);
			
			u64 remainingSize = sizeof(DirectoryIterator::searchPath) - (searchPathTail - subDirIterator.searchPath);
			
			memcpy_s(searchPathTail, remainingSize, iterator.filename.data(), iterator.filename.size());
			searchPathTail += iterator.filename.size();
			remainingSize  -= iterator.filename.size();
			
			ASSERT(remainingSize >= 3u);
			*searchPathTail++ = '\\';
			*searchPathTail++ = '*';
			*searchPathTail++ = '\0'; // the debug version of strcpy_s fills the buffer with 0xfe before copying			
			
			IndexDirectory(self, subDirIterator, self->index.size()-1u, currentPage, occupied);
		}
	}
		
	if (iterator.Failed())
		LogError("failed to search directory: '%s'. Last Error: %s", iterator.GetSearchPath().data(), StrLastErr(iterator.lastError));
}

static DWORD WINAPI ThreadProc(LPVOID param) {
	auto self = static_cast<SearchBarFiles*>(param);
	
	if (!self->head)
		 self->head = new SearchBarFiles::Page {.next = nullptr};
	memset(self->head->data, 0, sizeof(self->head->data));
	u64 occupied = 0u;
		
	DirectoryIterator iter {"."};
	IndexDirectory(self, iter, U64_MAX, self->head, occupied);
	
	LogTrace("Building index is complete. Size: %zu", self->index.size());
	return 0;
}


void SearchBarFiles::Open() {
	__super::Open();
	CancelIndexing(this);
	cancel = false;
	index.clear();
	hThread = CreateThread(NULL, 0, ThreadProc, this, 0, nullptr);
	if (hThread == NULL) {
		LogError("failed to start indexing thread. Last Error: %s", StrLastErr(GetLastError()));
		return;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBarFiles::FilterItems(std::string_view searchText) {
	filteredItems.clear();
	SetItemCount(0u);
	
	if (searchText.size() < 3u) return;
	
	DWORD exitCode = 0;
	if (!GetExitCodeThread(hThread, &exitCode)) return; // can happen if hThread is NULL. Something else went wrong beforehand
	if (exitCode == STILL_ACTIVE) return;

	for (const IndexEntry& indexEntry : index) {
		if (indexEntry.isDirectory) continue;
		
		FuzzyMatchResult matchResult {};
		if (!FuzzyMatch(searchText, indexEntry.filename, &matchResult))
			continue;

		// Keep the existing minimum number of matched characters behavior.
		if (matchResult.matchedCount < 3)
			continue;
		
		
		std::string fullPath {indexEntry.filename};
		
		const IndexEntry* currentIndexEntry = &indexEntry;
		
		// we need to compare with S64_MAX because the 'parent' member only uses 63 bits!
		while (currentIndexEntry->parent != S64_MAX) {
			currentIndexEntry = &index[currentIndexEntry->parent];
			
			const u64 extraSize = currentIndexEntry->filename.size() + 1u;
			fullPath.insert(fullPath.begin(), extraSize, '\0');
			
			fullPath[currentIndexEntry->filename.size()] = '\\';
			
			memcpy_s(
				fullPath.data(),
				fullPath.size(),
				currentIndexEntry->filename.data(),
				currentIndexEntry->filename.size());
		}
		
		Item item {
			.fullPath = std::move(fullPath),
			.filenameLength = indexEntry.filename.size(),
			.fuzzyMatchResult = matchResult};
			
		auto itWhere = std::find_if(filteredItems.begin(), filteredItems.end(), [&item] (const Item& other) {
			return CompareItems(item, other);
		});
		filteredItems.insert(itWhere, std::move(item));
	}

	SetItemCount(filteredItems.size());
}

void SearchBarFiles::OnUpdateItems(u64 firstVisible, u64 lastVisible) {

	ASSERT(firstVisible <= lastVisible)
	ASSERT(lastVisible <= filteredItems.size())

	for (u64 i = firstVisible; i < lastVisible; i++) {
		const Item& item = filteredItems[i];
				
		UpdateItem(i, UpdateItemParams {
			.text = GetFilename(item),
			.subText = item.fullPath,
			.matchedPosition = item.fuzzyMatchResult.position,
			.matchedLength = item.fuzzyMatchResult.length});
	}
}

void SearchBarFiles::OnPickItem(u64 i, const Event* event) {
	ASSERT(!event || (event->type == Event::Type_KeyPress));
	
	ASSERT(i < filteredItems.size());
	
	const Item& item = filteredItems[i];
	
	App::OpenBehavior openBehav = event
		? OpenBehaviorFromModifiers(event->keypress.mods)
		: App::OpenBehavior_Default;
	
	const bool ok = app.OpenEditor(item.fullPath, openBehav);
	if (!ok) LogError("OpenEditor() failed");
	
	shouldClose = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tools
//
//////////////////////////////////////////////////////////////////////////////////////////////////

void SearchBarTools::Init() {
	__super::Init("run tool...");
	filteredTools.reserve(Tool::tools.size());
	FilterItems({});
}
	
void SearchBarTools::FilterItems(std::string_view text) {
	
	filteredTools.clear();
	
	if (text.empty()) {
		for (const auto& t : Tool::tools)
			filteredTools.emplace_back(&t, FuzzyMatchResult {});
	
	} else {
	
		for (const Tool& tool : Tool::tools) {
			FuzzyMatchResult result {};
			if (FuzzyMatch(text, tool.name, &result)) {
				filteredTools.emplace_back(&tool, result);
			}
		}
	}
	
	SetItemCount(filteredTools.size());
}

static bool CheckIfAToolIsAlreadyRunnung(SearchBarTools* self) {
	
	if (app.toolOutput.process && app.toolOutput.process->IsRunning()) {
		MessageBoxA(mainWindow.hWnd,
			"There is already a tool running.\nCurrently only on tool at a time can be run.\nSorry!",
			"Tool already running",
			MB_OK | MB_ICONEXCLAMATION);
		return false;
	}
	
	return true;
}

void SearchBarTools::OnPickItem(u64 item, const Event* event) {
	ASSERT(item < filteredTools.size());
	
	const Tool& tool = Tool::tools[item];
	if (tool.forceConfiguration || (event && (event->keypress.mods & KM_Ctrl) != 0)) {
		ASSERT(!parameterConfigurator);
		parameterConfigurator = ParameterConfigurator::Make(tool.parameters);
		return;
	}
	
	if (!CheckIfAToolIsAlreadyRunnung(this))
		return;
	
	tool.GetDefaultValues(&app.toolOutput.toolParameterValues);
	app.toolOutput.tool = &tool;
	if (!app.toolOutput.StartProcess())
		LogError("failed to launch tool");
	
	shouldClose = true;
}

void SearchBarTools::OnUpdateItems(u64 firstVisible, u64 lastVisible) {
	for (u64 i = firstVisible; i < lastVisible; i++) {
		const Item& item = filteredTools[i];
		UpdateItem(i, UpdateItemParams {
			.text = item.tool->name,
			.subText = item.tool->command,
			.matchedPosition = item.fuzzyMatchResult.position,
			.matchedLength = item.fuzzyMatchResult.matchedCount});
	}
}

void SearchBarTools::OnFinishedParameterConfiguration() {
	ASSERT(parameterConfigurator->result != ParameterConfigurator::Result_Unfinished);
	
	if (parameterConfigurator->result == ParameterConfigurator::Result_Run) {
		if (!CheckIfAToolIsAlreadyRunnung(this)) return;
		
		ASSERT(selectedItem < filteredTools.size());	
		const Tool& tool = Tool::tools[selectedItem];
		
		parameterConfigurator->GetParameterValues(&app.toolOutput.toolParameterValues);
		app.toolOutput.tool = &tool;
		if (!app.toolOutput.StartProcess())
			LogError("failed to launch tool");
		
		shouldClose = true;
		
	} else {
		delete parameterConfigurator;
		parameterConfigurator = nullptr;
		shouldClose = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Commands
//
//////////////////////////////////////////////////////////////////////////////////////////////////

void SearchBarCommands::Init() {
	__super::Init("run command...");
	filteredCommands.reserve(Command::COUNT);
	FilterItems({});
}
	
void SearchBarCommands::FilterItems(std::string_view text) {
	
	filteredCommands.clear();
	
	if (text.empty()) {
		for (u64 i = 0u; i < Command::COUNT; i++)
			filteredCommands.emplace_back(static_cast<Command::Id>(i), FuzzyMatchResult {});
	
	} else {
	
		for (u64 i = 0u; i < Command::COUNT; i++) {
			FuzzyMatchResult result {};
			if (FuzzyMatch(text, commandDefinitions[i].name, &result)) {
				filteredCommands.emplace_back(static_cast<Command::Id>(i), result);
			}
		}
		
		std::sort(filteredCommands.begin(), filteredCommands.end(), [] (const Item& lhs, const Item& rhs) {
			if (const int cmp = CompareFuzzyMatchResults(lhs.fuzzyMatchResult, rhs.fuzzyMatchResult); cmp != 0)
				return cmp > 0;
				
			return lhs.commandId < rhs.commandId;
		});
	}
	
	SetItemCount(filteredCommands.size());
}

void SearchBarCommands::OnPickItem(u64 itemIdx, const Event* event) {
	ASSERT(itemIdx < filteredCommands.size());
	
	const Item& item = filteredCommands[itemIdx];
		
	const CommandDefinition& commandDef = commandDefinitions[item.commandId];
	if (commandDef.forceParameterConfiguration || (event && event->keypress.mods == KM_Ctrl)) {
		ASSERT(!parameterConfigurator);
		parameterConfigurator = ParameterConfigurator::Make(commandDef.parameters);
		return;
	}
	
	std::vector<ParameterValue> defaultValues {};
	ParameterDefinition::GetDefaultValues(commandDef.parameters, &defaultValues);
	
	app.HandleEvent(Event {
		.type = Event::Type_Command,
		.cmd = Command {
			.id = item.commandId,
			.parameters = defaultValues}});
	
	shouldClose = true;
}

void SearchBarCommands::OnUpdateItems(u64 firstVisible, u64 lastVisible) {
	for (u64 i = firstVisible; i < lastVisible; i++) {
		const Item& item = filteredCommands[i];
		const CommandDefinition& command = commandDefinitions[item.commandId];
		
		UpdateItem(i, UpdateItemParams {
			.text = command.name,
			.subText = command.description,
			.matchedPosition = item.fuzzyMatchResult.position,
			.matchedLength = item.fuzzyMatchResult.matchedCount});
	}
}

void SearchBarCommands::OnFinishedParameterConfiguration() {
	ASSERT(parameterConfigurator->result != ParameterConfigurator::Result_Unfinished);
	
	if (parameterConfigurator->result == ParameterConfigurator::Result_Run) {
		
		ASSERT(selectedItem < filteredCommands.size());
		const Item& item = filteredCommands[selectedItem];
		
		std::vector<ParameterValue> parameterValues {};
		parameterConfigurator->GetParameterValues(&parameterValues);
		
		app.HandleEvent(Event {
			.type = Event::Type_Command,
			.cmd = Command {
				.id = item.commandId,
				.parameters = parameterValues}});
		
		shouldClose = true;
		
	} else {
		delete parameterConfigurator;
		parameterConfigurator = nullptr;
		shouldClose = false;
	}
}
