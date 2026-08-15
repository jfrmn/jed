#pragma once

#include "ui/text-box.hh"
#include "ui/scrollarea.hh"

struct KeyEvent;
struct ParameterConfigurator;

struct SearchBar {

	//-----------------------------------------------------
	// types
	
	struct UpdateItemParams {
		std::string_view text = {};
		std::string_view subText = {};
		ID2D1Bitmap* icon = nullptr;
		
		u64 matchedPosition = 0u;
		u64 matchedLength = 0u;
	};

	//-----------------------------------------------------
	// data
	
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
	
	bool Init(std::string_view placeholderText, bool filterItemsImmediately);
	virtual ~SearchBar() noexcept;
	
	void UpdateItem(u64 i, const UpdateItemParams& params);
	void SetItemCount(u64 newItemCount);
	
	void OnUpdate();
	
	virtual void OnUpdateItems(u64 firstVisible, u64 lastVisible) = 0;
	
	void OnResize();
	void OnMouseWheel(f32 distance);
	void OnKeyDown(KeyEvent event, Command command);
	void OnChar(const char* data, u64 len);
	
	virtual void FilterItems(std::string_view text) = 0;
	virtual void AcceptItem(u64 item, const KeyEvent* event) = 0;
	virtual void OnFinishedParameterConfiguration() {};
};
