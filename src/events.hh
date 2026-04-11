#pragma once
#include "basic.hh"

#define VK_NONE 0
#define VK_MAX 255

struct D2D_RECT_F;

//-----------------------------------------------------------------------------
// Mouse event
//-----------------------------------------------------------------------------
struct MouseEvent {
	f32 x = 0.0f;
	f32 y = 0.0f;
};

//-----------------------------------------------------------------------------
// Key event
//-----------------------------------------------------------------------------
struct KeyEvent {
	u32  vkeycode = VK_NONE;
	bool ctrl  = false;
	bool shift = false;
	bool alt   = false;
	
	bool operator==(KeyEvent other) const;
	bool NoModifiers() const;
};

//-----------------------------------------------------------------------------
// Mouse
//-----------------------------------------------------------------------------

struct Mouse {
	
	enum Event {
		 Event_None    = 0,
		 Event_Down    = 1,
		 Event_Up      = 2
	};
	
	using OnClickFunction = void (*)(void* userdata, u64 userint);
	
	struct Element {
		void* userdata = nullptr;
		OnClickFunction onClickFunc = nullptr;
		
		bool operator==(const Element& other) const;
	};
	
	Event event = Event_None;
	
	f32 x = 0.0f;
	f32 y = 0.0f;
	bool isDown = false;
	bool isDragging = false;
	
	Element hotElementNext = {};
	Element hotElement = {};
	
	// free variables that can be set by whoever is dragging/clicking
	f32 dragArg = 0.0f;
	u64 onClickArg = 0u;
		
	bool Hittest(const D2D_RECT_F& area, void* userdata, OnClickFunction onClick, u64 arg = 0u);
	
	void StartDragging(f32 arg = 0.0f);
	
	void NextFrame();
};