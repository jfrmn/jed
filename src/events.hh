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
	
	using OnClickFunction = void (*)(void* hotElement, u64 userdata);
	
	Event event = Event_None;
	
	f32 x = 0.0f;
	f32 y = 0.0f;
	bool isDown = false;
	
	f32 dragStartX = 0.0f;
	f32 dragStartY = 0.0f;
	void* draggingElement = nullptr;
	
	void* hotElementNext = nullptr;
	void* hotElement = nullptr;
	OnClickFunction onClickFunc = nullptr;
	u64 onClickUserdata = 0u;
		
	bool Hittest(const D2D_RECT_F& area, void* element, OnClickFunction onClick = nullptr, u64 userdata = 0u);
	
	void StartDragging();
	bool IsDragging() const;
	
	void NextFrame();
};