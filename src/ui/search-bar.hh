#pragma once

#include "ui/text-box.hh"
#include "ui/scrollarea.hh"

struct KeyEvent;
struct ParameterConfigurator;

struct SearchBar {

	//-----------------------------------------------------
	// types
	
	struct ItemInfo {
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
	
	TextBox textBox = {};
	Scrollarea scrollarea = {};

	u64 itemCount = 0u;
	u64 selectedItem = U64_MAX;
	
	ParameterConfigurator* parameterConfigurator = nullptr;
	
	bool shouldClose = false;
	
	//-----------------------------------------------------
	// functions
	
	bool Init(std::string_view placeholderText, bool updateItemsImmediately);
	virtual void OnUpdate();
	void OnResize();
	
	void OnMouseWheel(f32 distance);
	void OnKeyDown(KeyEvent event);
	void OnChar(const char* data, u64 len);
	
	virtual u64 FilterItems(std::string_view text) = 0; // should return the new number of items
	virtual void AcceptItem(u64 item, const KeyEvent* event) = 0;
	virtual void GetItemInfo(u64 i, /*out*/ ItemInfo* itemInfo) = 0;
	virtual void OnFinishedParameterConfiguration() {};
	
	virtual ~SearchBar() noexcept;
};
