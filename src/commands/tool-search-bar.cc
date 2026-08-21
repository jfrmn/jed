#include "tool-search-bar.hh"
#include "tools.hh"
#include "main-window.hh"
#include "logging.hh"
#include "ui/parameter-configurator.hh"

ToolSearchBar* ToolSearchBar::Make() {
	auto self = new ToolSearchBar();
	self->Init("run tool...", true);
	self->filteredTools.reserve(Tool::tools.size());
	return self;
}
	
void ToolSearchBar::FilterItems(std::string_view text) {
	
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

static bool CheckIfAToolIsAlreadyRunnung(ToolSearchBar* self) {
	
	if (mainWindow.toolOutput.process && mainWindow.toolOutput.process->IsRunning()) {
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
	if (tool.forceConfiguration || (event && (event->modifiers & KM_Ctrl) != 0)) {
		ASSERT(!parameterConfigurator);
		parameterConfigurator = ParameterConfigurator::Make(tool.parameters);
		return;
	}
	
	if (!CheckIfAToolIsAlreadyRunnung(this))
		return;
	
	tool.GetDefaultValues(&mainWindow.toolOutput.toolParameterValues);
	mainWindow.toolOutput.tool = &tool;
	if (!mainWindow.toolOutput.StartProcess())
		LogError("failed to launch tool");
	
	shouldClose = true;
}

void ToolSearchBar::OnUpdateItems(u64 firstVisible, u64 lastVisible) {
	for (u64 i = firstVisible; i < lastVisible; i++) {
		const Item& item = filteredTools[i];
		UpdateItem(i, UpdateItemParams {
			.text = item.tool->name,
			.subText = item.tool->command,
			.matchedPosition = item.fuzzyMatchResult.position,
			.matchedLength = item.fuzzyMatchResult.matchedCount});
	}
}

void ToolSearchBar::OnFinishedParameterConfiguration() {
	ASSERT(parameterConfigurator->result != ParameterConfigurator::Result_Unfinished);
	
	if (parameterConfigurator->result == ParameterConfigurator::Result_Run) {
		if (!CheckIfAToolIsAlreadyRunnung(this)) return;
		
		ASSERT(selectedItem < filteredTools.size());	
		const Tool& tool = Tool::tools[selectedItem];
		
		parameterConfigurator->GetParameterValues(&mainWindow.toolOutput.toolParameterValues);
		mainWindow.toolOutput.tool = &tool;
		if (!mainWindow.toolOutput.StartProcess())
			LogError("failed to launch tool");
		
		shouldClose = true;
		
	} else {
		delete parameterConfigurator;
		parameterConfigurator = nullptr;
		shouldClose = false;
	}
}
