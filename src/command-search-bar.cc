#include "command-search-bar.hh"
#include "commands.hh"
#include "events.hh"
#include "main-window.hh"
#include "commands/parameter-configurator.hh"

#include <algorithm>

CommandSearchBar* CommandSearchBar::Make() {
	auto self = new CommandSearchBar();
	self->Init("run command...", true);
	self->filteredCommands.reserve(Command::COUNT);
	return self;
}
	
void CommandSearchBar::FilterItems(std::string_view text) {
	
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

void CommandSearchBar::AcceptItem(u64 itemIdx, const KeyEvent* event) {
	ASSERT(itemIdx < filteredCommands.size());
	
	const Item& item = filteredCommands[itemIdx];
		
	const CommandDefinition& command = commandDefinitions[item.commandId];
	if (command.forceParameterConfiguration || (event && event->modifiers == KM_Ctrl)) {
		ASSERT(!parameterConfigurator);
		parameterConfigurator = ParameterConfigurator::Make(command.parameters);
		return;
	}
	
	std::vector<ParameterValue> defaultValues {};
	ParameterDefinition::GetDefaultValues(command.parameters, &defaultValues);
	mainWindow.OnKeyEvent(KeyEvent {VK_NONE, KM_None}, Command {item.commandId, defaultValues.data()});
	
	shouldClose = true;
}

void CommandSearchBar::OnUpdateItems(u64 firstVisible, u64 lastVisible) {
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

void CommandSearchBar::OnFinishedParameterConfiguration() {
	ASSERT(parameterConfigurator->result != ParameterConfigurator::Result_Unfinished);
	
	if (parameterConfigurator->result == ParameterConfigurator::Result_Run) {
		
		ASSERT(selectedItem < filteredCommands.size());
		const Item& item = filteredCommands[selectedItem];
		
		std::vector<ParameterValue> parameterValues {};
		parameterConfigurator->GetParameterValues(&parameterValues);
		mainWindow.OnKeyEvent(KeyEvent {VK_NONE, KM_None}, Command {item.commandId, parameterValues.data()});
		
		shouldClose = true;
		
	} else {
		delete parameterConfigurator;
		parameterConfigurator = nullptr;
		shouldClose = false;
	}
}

