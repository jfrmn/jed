#pragma once
#include "ui/search-bar.hh"
#include "util/string-util.hh"
#include <vector>

struct KeyEvent;

struct FileSearchBar : public SearchBar {

	//-----------------------------------------------------
	// data
	struct Item {
		std::string fullPath = {};
		u64 filenameLength = 0u;
		FuzzyMatchResult fuzzyMatchResult = {};
	};

	//-----------------------------------------------------
	// data
	
	std::vector<Item> items = {};	
	
	//-----------------------------------------------------
	// functions

	static FileSearchBar* Make();
	
	virtual u64 FilterItems(std::string_view text) override;
	virtual void AcceptItem(u64 item, const KeyEvent* event) override;
	virtual void GetItemInfo(u64 i, /*out*/ ItemInfo* itemInfo) override;
};
