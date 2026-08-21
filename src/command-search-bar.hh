#pragma once
#include "ui/search-bar.hh"
#include "util.hh"
#include "commands.hh"

struct CommandSearchBar : public SearchBar {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	struct Item {
		Command::Id commandId = Command::Id_None;
		FuzzyMatchResult fuzzyMatchResult = {};	
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	std::vector<Item> filteredCommands = {};
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	static CommandSearchBar* Make();

	virtual void FilterItems(std::string_view text) override;
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;	
	virtual void AcceptItem(u64 item, const KeyEvent* event) override;
	virtual void OnFinishedParameterConfiguration() override;	

};
