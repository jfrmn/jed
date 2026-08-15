#pragma once
#include "basic.hh"

#define VK_NONE 0
#define VK_MAX 255

struct D2D_RECT_F;
struct ParameterValue;

//-----------------------------------------------------------------------------
// Key event
//-----------------------------------------------------------------------------

enum KeyModifier : u32 {
	KM_None  = 0,
	KM_Ctrl  = 1,
	KM_Shift = 2,
	KM_Alt   = 4
};

struct KeyEvent {
	u32 vkeycode = VK_NONE;
	u32 modifiers = KM_None;
	
	bool operator==(KeyEvent other) const;
};

//-----------------------------------------------------------------------------
// File changed event
//-----------------------------------------------------------------------------

struct FileChangeRecord {
	// should match the constants in windows.h
	enum Action {
	 	 Action_Unknown    = 0,
	 	 Action_Added      = 1,
	 	 Action_Removed    = 2,
	 	 Action_Modified   = 3,
	 	 Action_RenamedOld = 4,
	 	 Action_RenamedNew = 5,
	};

	Action action = Action_Unknown;
	char*  filename = nullptr;
	u64    filenameLength = 0u;
};

struct FileChangedEvent {
	
	char* directory = nullptr;
	u64   directoryLength = 0u;
	u64   longestFilenameLength = 0u;
	
	u64 recordCount = 0u;
	FileChangeRecord records[1] = {};	
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