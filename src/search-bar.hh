#pragma once
#include "ui/text-box.hh"
#include "ui/scrollarea.hh"
#include "commands.hh"
#include "util.hh"

#include <atomic>
#include <string_view>

struct Event;
struct ParameterConfigurator;
struct Tool;
struct Language;

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
		std::string_view prefix = {};
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
	virtual void Open();
	
	void UpdateItem(u64 i, const UpdateItemParams& params);
	void SetItemCount(u64 newItemCount);
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) = 0;
	
	void OnResize();
	bool HandleEvent(const Event& event);
	void OnMouseWheel(f32 distance);
	
	virtual void FilterItems(std::string_view text) = 0;
	virtual void OnPickItem(u64 item, const Event* event) = 0;
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
	
	struct IndexEntry {
		std::string_view filename = {};
		u64  parent      : 63 = 0u;
		bool isDirectory : 1  = false;
	};
	
	struct Page {
		static constexpr u64 SIZE = 4096 - sizeof(Page*);
		Page* next = nullptr;
		char data[SIZE];
	};

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	Page* head = nullptr;
	std::vector<IndexEntry> index = {};
	
	std::atomic_bool cancel = false;	
	void* hThread = nullptr;
	
	std::vector<Item> filteredItems = {};
		
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	void Init();
	virtual ~SearchBarFiles() noexcept;
	virtual void Open() override;
	
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;
	
	virtual void FilterItems(std::string_view text) override;
	virtual void OnPickItem(u64 item, const Event* event) override;
	
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
		const Language* langauge = nullptr;
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
	virtual void Open() override;
			
	virtual void FilterItems(std::string_view text) override;
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) override;	
	virtual void OnPickItem(u64 item, const Event* event) override;
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
	virtual void OnPickItem(u64 item, const Event* event) override;
	virtual void OnFinishedParameterConfiguration() override;	

};
