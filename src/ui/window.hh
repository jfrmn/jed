#pragma once
#include "events.hh"

#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

struct Editor;
struct ID2D1HwndRenderTarget;
struct ID2D1DeviceContext;

struct Window {

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	struct CreateParams {
		std::string_view className = {};
		std::string_view title = {};
		int width = 0;
		int height = 0;
		HWND hwndParent = NULL;
	};
		
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	HWND hWnd = NULL;
	ID2D1HwndRenderTarget* renderTarget = nullptr;
	ID2D1DeviceContext* deviceContext = nullptr;

	float width  = .0f;
	float height = .0f;
	
	bool destroyRecieved = false;
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	void CleanUp();
	
	void OnUpdate();
	
	s64  SendFunctionCall(void* userdata, s64  (*func)(void*));
	void PostFunctionCall(void* userdata, void (*func)(void*));
	
	void SendUpdate();
	void PostUpdate();
	
	void PostFileChangedEvent(FileChangedEvent* fileChangedEvent);
	
	virtual bool OnMouseWheel(float wheel)         { return false; }
	virtual void OnKeyDown(KeyEvent event)         {}
	virtual void OnKeyUp(KeyEvent event)           {}
	virtual void OnChar(const char* utf8, u64 len) {}
	virtual void OnResize(f32 newW, f32 newH)      {}
	virtual bool OnClose()                         { return true; }
	virtual void OnFileChanged(FileChangedEvent* fileChangedEvent) {}
	
protected:
	bool Create(const CreateParams& createParams);
};
