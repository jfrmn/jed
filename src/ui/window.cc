#include "window.hh"
#include "globals.hh"

#include "graphics/factories.hh"
#include "util/logging.hh"
#include "util/string-util.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>
#include <dwmapi.h>

static u32 wmUserUpdate       = 0u;
static u32 wmUserSendFuncCall = 0u;
static u32 wmUserPostFuncCall = 0u;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static LRESULT WINAPI WindowProc(
	HWND hWnd,
	UINT nMSG,
	WPARAM wParam,
	LPARAM lParam);

bool Window::Create(const CreateParams &createParams) {

	HINSTANCE hInstance = GetModuleHandle(0);

	WNDCLASSA wndc = { sizeof(WNDCLASSA) };
	wndc.style = CS_HREDRAW | CS_VREDRAW;
	wndc.hInstance = hInstance;
	wndc.hbrBackground = NULL; //(HBRUSH)GetStockObject(BLACK_BRUSH);
	wndc.lpszClassName = createParams.className.data();
	wndc.lpfnWndProc = WindowProc;
	wndc.hCursor = LoadCursor(NULL, IDC_ARROW);
	
	// @TODO see comment at WM_CHAR
	if (!RegisterClassA(&wndc)) {
		LogError("RegisterClass() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}

	HWND hWnd = CreateWindowA(
		createParams.className.data(),           // class name 
		createParams.title.data(),               // title
		WS_OVERLAPPEDWINDOW,                     // style
		CW_USEDEFAULT, CW_USEDEFAULT,            // positon
		createParams.width, createParams.height, // size
		createParams.hwndParent,                 // parent hwnd
		NULL,                                    // menu
		hInstance,                               // instance
		NULL);                                   // lparam for init-message

	if (!hWnd) {
		LogError("CreateWindow() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}

	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	const long w = clientRect.right  - clientRect.left;
	const long h = clientRect.bottom - clientRect.top;

	ID2D1HwndRenderTarget* hwndRenderTarget = nullptr;

	if (HRESULT hr = d2dFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hWnd, D2D1::SizeU(w, h)),
			&hwndRenderTarget);
			hr != S_OK) {
		LogError("CreateHwndRenderTarget() failed. HRESULT: %", FHr(hr));
		return false;
	}

	hwndRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	hwndRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	
	if (HRESULT hr = hwndRenderTarget->QueryInterface(&deviceContext); hr != S_OK) {
		LogError("QueryInterface() for ID2D1DeviceContext failed. HRESULT: %", FHr(hr));
		return false;
	}

	BOOL useDarkMode = TRUE;
	if (HRESULT hr = DwmSetWindowAttribute(
			hWnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE,
			&useDarkMode,
			sizeof(useDarkMode));
			hr != S_OK) {
		LogError("DwmSetWindowAttribute() failed. HRESULT: %", FHr(hr));
		return false;
	}
	
	wmUserUpdate       = RegisterWindowMessageA("WM_USER_UPDATE");
	wmUserSendFuncCall = RegisterWindowMessageA("WM_USER_SENDFUNCCALL");
	wmUserPostFuncCall = RegisterWindowMessageA("WM_USER_POSTFUNCCALL");

	this->hWnd = hWnd;
	this->renderTarget = hwndRenderTarget;
	this->width = static_cast<float>(w);
	this->height = static_cast<float>(h);
	
	return true;
}

void Window::CleanUp() {

	if (deviceContext) {
		deviceContext->Release();
		deviceContext = nullptr;
	}
	
	if (renderTarget) {
		renderTarget->Release();
		renderTarget = nullptr;
	}
	
	SetWindowLongPtr(hWnd, GWLP_USERDATA, 0u);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
s64 Window::SendFunctionCall(void* userdata, s64 (*func)(void*)) {
	const auto wparam = reinterpret_cast<WPARAM>(userdata);
	const auto lparam = reinterpret_cast<LPARAM>(func);
		
	return SendMessageA(hWnd, wmUserSendFuncCall, wparam, lparam);
}

void Window::PostFunctionCall(void* userdata, void (*func)(void*)) {
	const auto wparam = reinterpret_cast<WPARAM>(userdata);
	const auto lparam = reinterpret_cast<LPARAM>(func);
		
	PostMessageA(hWnd, wmUserPostFuncCall, wparam, lparam);
}

void Window::SendUpdate() {
	SendMessageA(hWnd, wmUserUpdate, 0, 0);
}

void Window::PostUpdate() {
	PostMessageA(hWnd, wmUserUpdate, 0, 0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static float GetXFromLParam(LPARAM lParam) {
	const int x = LOWORD(lParam);
	return static_cast<float>(x);
}

static float GetYFromLParam(LPARAM lParam) {
	const int y = HIWORD(lParam);
	return static_cast<float>(y);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LRESULT __stdcall WindowProc(HWND hWnd, UINT nMSG, WPARAM wParam, LPARAM lParam) {
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	
	if (!window)
		return DefWindowProc(hWnd, nMSG, wParam, lParam);

	switch (nMSG) {

		case WM_MOUSEMOVE: {
			const float x = GetXFromLParam(lParam);
			const float y = GetYFromLParam(lParam);
		
			mouse.x = x;
			mouse.y = y;
			return 0l;
		} break;
		
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP: {
			const float x = GetXFromLParam(lParam);
			const float y = GetYFromLParam(lParam);
			mouse.x = x;
			mouse.y = y;
			
			const MouseEvent event {x, y};
			if (nMSG == WM_LBUTTONDOWN) {
				mouse.isDown = true;
				mouse.event = Mouse::Event_Down;
				window->OnMouseDown(event);
			
			} else if (nMSG == WM_LBUTTONUP) {
				mouse.isDown = false;
				mouse.event = Mouse::Event_Up;
				window->OnMouseUp(event);
			}
				
		} break;
		
		case WM_MOUSEWHEEL: {
			const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			const float distance = static_cast<float>(wheelDelta) / WHEEL_DELTA;
			window->OnMouseWheel(distance);
		} break;
			
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_KEYDOWN: {
			KeyEvent keyEvent {
				.vkeycode = static_cast<u32>(wParam),
				.ctrl  = HIWORD(GetKeyState(VK_CONTROL)) != 0,
				.shift = HIWORD(GetKeyState(VK_SHIFT))   != 0,
				.alt   = HIWORD(GetKeyState(VK_MENU))    != 0};

			if      (nMSG == WM_KEYDOWN || nMSG == WM_SYSKEYDOWN) window->OnKeyDown(keyEvent);
			else if (nMSG == WM_KEYUP   || nMSG == WM_SYSKEYUP)   window->OnKeyUp(keyEvent);
			else ASSERT_UNREACHABLE
			return 0L;
		} break;

		case WM_SYSCHAR: {
			// This message is sent when ALT+some key is pressed.
			// The DefWindowProc tries to open menus or execute menu items (which we don't have) and plays a gong sound
			// when no menu is found. To avoid this sound we need to handle.
			return 0L;
		} break;

		case WM_CHAR: {
			const wchar wch = static_cast<wchar>(wParam);
			
			if ((wch >= L'\0' && wch <= L'\x1f') ||               // ascii-control character block 
				(wch == L'\x7f') ||                               // delete
				(wch == L' ' && HIWORD(GetKeyState(VK_CONTROL)))) // Ctrl+Space is used for autocomplete. We need to supress this space
				break;
			
			// @FIXME we need to use the Unicode version of RegisterClass (RegisterClassW) in order to recieve utf16 chars
			// the A-Variant just gives us characters in the current code-page
			
			// 6 is the theroretical maximum for utf8
			char utf8Buffer[6] {'\0'};
			u64  utf8Len = 0u;
			
			if (!ToUtf8(std::wstring_view {&wch, 1u}, utf8Buffer, &utf8Len)) {
				LogWarning("failed to convert input to utf-8");
				return false;
			}
		
			window->OnChar(utf8Buffer, utf8Len);
		} break;

		case WM_SIZE: {
			const u32 uwidth  = LOWORD(lParam);
			const u32 uheight = HIWORD(lParam);
			
			const f32 width  = static_cast<float>(uwidth);
			const f32 height = static_cast<float>(uheight);

			window->width = width;
			window->height = height;
			window->renderTarget->Resize(D2D_SIZE_U {uwidth, uheight});
			
			window->OnResize(window->width, window->height);
		} break;
		
		case WM_CLOSE: {
			const bool shouldClose = window->OnClose();
			if (shouldClose)
				DestroyWindow(window->hWnd);
		} break;
		
		case WM_DESTROY: {
			window->destroyRecieved = true;
			PostQuitMessage(0);
		} break;
				
		default: break;
	}
	
	
	if (nMSG == wmUserUpdate) {
		// we update after every message so don't have to do anything
	
	} else if (nMSG == wmUserSendFuncCall) {
		auto userdata = reinterpret_cast<void*>(wParam);
		auto func = reinterpret_cast<s64 (*)(void*)>(lParam);
		const s64 result = func(userdata);
		return result;
	
	} else if (nMSG == wmUserPostFuncCall) {
		auto userdata = reinterpret_cast<void*>(wParam);
		auto func = reinterpret_cast<void (*)(void*)>(lParam);
		func(userdata);
		return 1;
	}

	return DefWindowProc(hWnd, nMSG, wParam, lParam);
}
