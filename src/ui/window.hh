#pragma once
#include "events.hh"

#include <string_view>

struct Editor;
struct ID2D1HwndRenderTarget;
struct ID2D1DeviceContext;
struct HWND__;

#define MAIN_WINDOW_WNDCLASSNAME "jedmainwnd"

struct Window {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	struct CreateParams {
		std::string_view className = {};
		std::string_view title = {};
		int width = 0;
		int height = 0;
		HWND__* hWndParent = NULL;
	};
		
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	HWND__* hWnd = NULL;
	ID2D1HwndRenderTarget* renderTarget = nullptr;
	ID2D1DeviceContext* deviceContext = nullptr;

	f32 width  = .0f;
	f32 height = .0f;
	bool quitReceived = false;
	
	Event event = {};
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	bool Create(const CreateParams& createParams);
	void Show();
	void ClearEvent();
	
	void Destroy();
	void CleanUp();
		
	s64  SendFunctionCall(void* userdata, s64  (*func)(void*));
	void PostFunctionCall(void* userdata, void (*func)(void*));
	
	void SendUpdate();
	void PostUpdate();
	
	void PostFileChangedEvent(FileChangedEvent* fileChangedEvent);	
};

extern Window mainWindow;
