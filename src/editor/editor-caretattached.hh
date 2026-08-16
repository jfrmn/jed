#pragma once
#include "text/text-position.hh"
#include <atomic>
#include <string>

struct KeyEvent;
struct Editor;
struct D2D_POINT_2F;
struct ID2D1DeviceContext;

constexpr f32 TOOLTIP_CURSOR_EXTRA_OFFSET_Y = 5.0f;

struct EditorCaretAttached {
	
	//-------------------------------------------
	// types
		
	enum State {
		 State_Unknown = 0,
		 State_Fetching,
		 State_Errored,
		 State_NoItems,
		 State_Completed
	};
	
	//-------------------------------------------
	// data
	
	Editor* owner = nullptr;
	TextPosition triggerPosition = {};
	
	std::atomic<State> state = State_Unknown;
	std::atomic_int references = 0;
	
	std::string error = {};
	
	//-------------------------------------------
	// functions
	
	void AddReference();
	void RemoveReference();
	bool DrawIncompleteState(ID2D1DeviceContext* deviceContext, std::string_view noItemsText = "No items.");
	D2D_POINT_2F GetPosition() const;
	
	virtual void OnUpdate() = 0;
	virtual void OnInput() = 0;
	virtual bool OnKeyEvent(KeyEvent event) = 0;	
		
	virtual ~EditorCaretAttached() noexcept {} 
};