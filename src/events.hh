#pragma once
#include "basic.hh"
#include "commands.hh"

struct D2D_RECT_F;
struct D2D_POINT_2F;
struct D2D_SIZE_F;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Constants
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#define VK_NONE 0
#define VK_MAX 255

enum KeyModifier : u32 {
	KM_None  = 0,
	KM_Ctrl  = 1,
	KM_Shift = 2,
	KM_Alt   = 4
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// FileChangedRecord
//
///////////////////////////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Event
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct Event {
	
	enum Type {
		 Type_None = 0,
		 Type_KeyPress,
		 Type_Text,
		 Type_MouseDown,
		 Type_MouseUp,
		 Type_MouseWheel,
		 Type_Resize,
		 Type_Close,
		 Type_FileChange,
		 Type_Command
	};
	
	Type type = Type_None;
	
	// could make this a union
	// but msvc can't handle multiple anonymus structs inside a union (internal compiler error)
	// we could use named structs but member access is nicer this way
	// and but we don't allocate this struct often
	
	union {		
		// KeyPress
		struct {
			u32 vkc;
			u32 mods;
		} keypress;
		
		// Text
		struct {
			char data[6]; // 6 is the theroretical maximum for utf8
			u64 len = 0u;
		} text;
		
		// MouseDown, MouseUp
		struct {
			f32 x;
			f32 y;
		} mouse;
		
		// MouseWheel
		f32 wheelDistance;
		
		// Resize
		struct {
			f32 w;
			f32 h;
		} newSize;
	
		// FileChangedEvent
		FileChangedEvent* fileChangedEvent = nullptr;
		
		// Command
		Command cmd;
	};
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Mouse State
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct MouseState {
	using Callback = void (*)(void* userdata, u64 userint);
	
	struct Element {
		Callback callback = nullptr;
		void* userdata = nullptr;
		u64 userint = 0u;
		
		bool operator==(const Element& other) const = default;
	};
	
	f32 x = 0.0f;
	f32 y = 0.0f;
	bool isDown = false;
	bool isDragging = false;
	
	Element nextHotElement = {};
	Element currentHotElement = {};
	f32 dragDeltaX = 0.0f;
	f32 dragDeltaY = 0.0f;
	
	// preform a hittest and, if successfull, calls Hot()
	bool Hittest(const D2D_RECT_F& area, void* userdata, Callback onClick = nullptr, u64 userint = 0u);
	// Set as the nextHotElement. Return if true if the current Hot element
	bool Hot(void* userdata, Callback onClick = nullptr, u64 userint = 0u);
	
	void StartDragging(f32 dx = 0.0f, f32 dy = 0.0f);
	
	void NextFrame(const Event& event);
};

extern MouseState mouse;
