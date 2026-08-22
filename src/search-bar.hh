#pragma once
#include "ui/text-box.hh"
#include "ui/scrollarea.hh"
#include "util/rc.h"
#include "util.hh"

#include <atomic>
#include <mutex>
#include <string_view>

struct KeyEvent;
struct ParameterConfigurator;
struct Tool;

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// SearchBar
//
//////////////////////////////////////////////////////////////////////////////////////////////////

struct SearchBar {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
	
	struct UpdateItemParams {
		std::string_view text = {};
		std::string_view subText = {};
		ID2D1Bitmap* icon = nullptr;
		
		u64 matchedPosition = 0u;
		u64 matchedLength = 0u;
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	D2D_RECT_F area = {};
	
	f32 itemHighlightAnimationValue = .0f;
	f32 spawnAnimationValue = .0f;
	
	TextBox textBox = {};
	Scrollarea scrollarea = {};

	u64 itemCount = 0u;
	u64 selectedItem = U64_MAX;
	
	ParameterConfigurator* parameterConfigurator = nullptr;
	
	bool shouldClose = false;
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
protected:
	void Init(std::string_view placeholderText);
	virtual ~SearchBar() noexcept;

public:
	void OnUpdate();
	
	void UpdateItem(u64 i, const UpdateItemParams& params);
	void SetItemCount(u64 newItemCount);
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) = 0;
	
	void OnResize();
	void OnMouseWheel(f32 distance);
	void OnKeyDown(KeyEvent event, Command command);
	void OnChar(const char* data, u64 len);
	
	virtual void FilterItems(std::string_view text) = 0;
	virtual void OnPickItem(u64 item, const KeyEvent* event) = 0;
	virtual void OnFinishedParameterConfiguration() {};
};

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Files
//
//////////////////////////////////////////////////////////////////////////////////////////////////

struct SearchBarFiles : public SearchBar {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
	
	struct Item {
		std::string fullPath = {};
		u64 filenameLength = 0u;
		FuzzyMatchResult fuzzyMatchResult = {};
	};
	
	struct ThreadData {
		SearchBarFiles* searchBar = nullptr;
		
		std::string searchTerm = {};
	
		std::mutex mtxResults = {};		
		std::vector<Item> results = {};
		
		std::atomic_bool isCancelled = false;
		std::atomic_bool isComplete = false;
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	void* hThread = nullptr;
	Rc<ThreadData> threadData = {};	
		
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	void Init();
	virtual ~SearchBarFiles() noexcept;
	
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;
	
	virtual void FilterItems(std::string_view text) override;
	virtual void OnPickItem(u64 item, const KeyEvent* event) override;
	
};

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tools
//
//////////////////////////////////////////////////////////////////////////////////////////////////


struct SearchBarTools : public SearchBar {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
	
	struct Item {
		const Tool* tool = nullptr;
		FuzzyMatchResult fuzzyMatchResult = {};	
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	std::vector<Item> filteredTools = {};
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	void Init();
		
	virtual void FilterItems(std::string_view text) override;
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;	
	virtual void OnPickItem(u64 item, const KeyEvent* event) override;
	virtual void OnFinishedParameterConfiguration() override;	
};

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// Commands
//
//////////////////////////////////////////////////////////////////////////////////////////////////

struct SearchBarCommands : public SearchBar {

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

	void Init();

	virtual void FilterItems(std::string_view text) override;
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;	
	virtual void OnPickItem(u64 item, const KeyEvent* event) override;
	virtual void OnFinishedParameterConfiguration() override;	

};
