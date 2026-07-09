#pragma once
#include "ui/search-bar.hh"
#include "ui/text-box.hh"
#include "util/string-util.hh"

struct Tool;
struct ParameterConfigurator;

struct ToolSearchBar : public SearchBar {

	//-----------------------------------------------------
	// types
	
	struct Item {
		const Tool* tool = nullptr;
		FuzzyMatchResult fuzzyMatchResult = {};	
	};
	
	//-----------------------------------------------------
	// data
	
	std::vector<Item> filteredTools = {};
	
	//-----------------------------------------------------
	// functions

	static ToolSearchBar* Make();
		
	virtual void FilterItems(std::string_view text) override;
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;	
	virtual void AcceptItem(u64 item, const KeyEvent* event) override;
	virtual void OnFinishedParameterConfiguration() override;	
};
