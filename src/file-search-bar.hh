#pragma once
#include "ui/search-bar.hh"
#include "util.hh"
#include <vector>
#include <mutex>

struct KeyEvent;

struct FileSearchBar : public SearchBar {

	//-----------------------------------------------------
	// types
	
	struct Item {
		std::string fullPath = {};
		u64 filenameLength = 0u;
		FuzzyMatchResult fuzzyMatchResult = {};
	};
	
	struct ThreadData {
		FileSearchBar* searchBar = nullptr;
		
		std::string searchTerm = {};
	
		std::mutex mtxResults = {};		
		std::vector<Item> results = {};
		
		std::atomic_int  references = 0;
		std::atomic_bool isCancelled = false;
		std::atomic_bool isComplete = false;
		
		void RemoveReference();
	};

	//-----------------------------------------------------
	// data
	
	void* hThread = nullptr;
	ThreadData* threadData = nullptr;	
		
	//-----------------------------------------------------
	// functions

	static FileSearchBar* Make();
	virtual ~FileSearchBar() noexcept;
	
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;
	
	virtual void FilterItems(std::string_view text) override;
	virtual void AcceptItem(u64 item, const KeyEvent* event) override;
	
};
