#include "tool-search-bar.hh"
#include "tools.hh"
#include "parameter-configurator.hh"
#include "main-window.hh"

#include "util/logging.hh"

ToolSearchBar* ToolSearchBar::Make() {
	auto self = new ToolSearchBar();
	self->Init("run tool...", true);
	self->filteredTools.reserve(Tool::tools.size());
	return self;
}
	
u64 ToolSearchBar::FilterItems(std::string_view text) {
	
	filteredTools.clear();
	
	if (text.empty()) {
		for (const auto& t : Tool::tools)
			filteredTools.emplace_back(&t, FuzzyMatchResult {});
		
		return filteredTools.size();
	}
	
	for (const Tool& tool : Tool::tools) {
		
		FuzzyMatchResult result {};
		if (FuzzyMatch(text, tool.name, &result)) {
			filteredTools.emplace_back(&tool, result);
		}
	}
	
	return filteredTools.size();
}

static bool CheckIfAToolIsAlreadyRunnung(ToolSearchBar* self) {
	
	if (mainWindow.console.process && mainWindow.console.process->IsRunning()) {
		MessageBoxA(mainWindow.hWnd,
			"There is already a tool running.\nCurrently only on tool at a time can be run.\nSorry!",
			"Tool already running",
			MB_OK | MB_ICONEXCLAMATION);
		return false;
	}
	
	return true;
}

void ToolSearchBar::AcceptItem(u64 item, const KeyEvent* event) {
	ASSERT(item < filteredTools.size());
	
	const Tool& tool = Tool::tools[item];
	if (tool.forceConfiguration || (event && event->ctrl)) {
		ASSERT(!parameterConfigurator);
		parameterConfigurator = ParameterConfigurator::Make(tool.parameters);
		return;
	}
	
	if (!CheckIfAToolIsAlreadyRunnung(this))
		return;
	
	tool.GetDefaultValues(&mainWindow.console.toolParameterValues);
	mainWindow.console.tool = &tool;
	if (!mainWindow.console.StartProcess())
		LogError("failed to launch tool");
	
	shouldClose = true;
}

void ToolSearchBar::GetItemInfo(u64 i, /*out*/ ItemInfo* itemInfo) {
	ASSERT(i < filteredTools.size());
	const Item& item = filteredTools[i];
	*itemInfo = ItemInfo {
		.text = item.tool->name,
		.subText = item.tool->description,
		.matchedPosition = item.fuzzyMatchResult.position,
		.matchedLength = item.fuzzyMatchResult.matchedCount};
}

void ToolSearchBar::OnFinishedParameterConfiguration() {
	ASSERT(parameterConfigurator->result != ParameterConfigurator::Result_Unfinished);
	
	if (parameterConfigurator->result == ParameterConfigurator::Result_Run) {
		if (!CheckIfAToolIsAlreadyRunnung(this)) return;
		
		ASSERT(selectedItem < filteredTools.size());	
		const Tool& tool = Tool::tools[selectedItem];
		
		parameterConfigurator->GetParameterValues(&mainWindow.console.toolParameterValues);
		mainWindow.console.tool = &tool;
		if (!mainWindow.console.StartProcess())
			LogError("failed to launch tool");
		
		shouldClose = true;
		
	} else {
		delete parameterConfigurator;
		parameterConfigurator = nullptr;
		shouldClose = false;
	}
}
