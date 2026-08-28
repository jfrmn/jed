#include "window.hh"
#include "globals.hh"
#include "graphics.hh"
#include "logging.hh"
#include "util.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d2d1_1.h>
#include <dwmapi.h>

static u32 wmUserUpdate       = 0u;
static u32 wmUserSendFuncCall = 0u;
static u32 wmUserPostFuncCall = 0u;
static u32 wmUserFileChanged  = 0u;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Window mainWindow = {};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static LRESULT WINAPI WindowProc(
	HWND hWnd,
	UINT nMSG,
	WPARAM wParam,
	LPARAM lParam);

bool Window::Create(const CreateParams& createParams) {

	HINSTANCE hInstance = GetModuleHandle(0);

	WNDCLASSA wndc = { sizeof(WNDCLASSA) };
	wndc.style = CS_HREDRAW | CS_VREDRAW;
	wndc.hInstance = hInstance;
	wndc.hbrBackground = NULL;
	wndc.lpszClassName = createParams.className.data();
	wndc.lpfnWndProc = WindowProc;
	wndc.hCursor = LoadCursor(NULL, IDC_ARROW);
	
	// @TODO see comment at WM_CHAR
	if (!RegisterClassA(&wndc)) {
		LogError("RegisterClass() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}

	HWND hWnd = CreateWindowA(
		createParams.className.data(),           // class name 
		createParams.title.data(),               // title
		WS_OVERLAPPEDWINDOW,                     // style
		CW_USEDEFAULT, CW_USEDEFAULT,            // positon
		createParams.width, createParams.height, // size
		createParams.hWndParent,                 // parent hwnd
		NULL,                                    // menu
		hInstance,                               // instance
		NULL);                                   // lparam for init-message

	if (!hWnd) {
		LogError("CreateWindow() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}

	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	const u32 w = clientRect.right  - clientRect.left;
	const u32 h = clientRect.bottom - clientRect.top;

	ID2D1HwndRenderTarget* hwndRenderTarget = nullptr;

	if (HRESULT hr = d2dFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hWnd, D2D_SIZE_U {w, h}),
			&hwndRenderTarget);
			hr != S_OK) {
		LogError("CreateHwndRenderTarget() failed. HRESULT: %s", StrHr(hr));
		return false;
	}

	hwndRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	hwndRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	
	if (HRESULT hr = hwndRenderTarget->QueryInterface(&deviceContext); hr != S_OK) {
		LogError("QueryInterface() for ID2D1DeviceContext failed. HRESULT: %s", StrHr(hr));
		return false;
	}

	BOOL useDarkMode = TRUE;
	if (HRESULT hr = DwmSetWindowAttribute(
			hWnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE,
			&useDarkMode,
			sizeof(useDarkMode));
			hr != S_OK) {
		LogError("DwmSetWindowAttribute() failed. HRESULT: %s", StrHr(hr));
		return false;
	}
	
	wmUserUpdate       = RegisterWindowMessageA("WM_USER_UPDATE");
	wmUserSendFuncCall = RegisterWindowMessageA("WM_USER_SENDFUNCCALL");
	wmUserPostFuncCall = RegisterWindowMessageA("WM_USER_POSTFUNCCALL");
	wmUserFileChanged  = RegisterWindowMessageA("WM_USER_FILECHANGED");

	this->hWnd = hWnd;
	this->renderTarget = hwndRenderTarget;
	this->width = static_cast<f32>(w);
	this->height = static_cast<f32>(h);
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Window::Show() {
	ShowWindow(hWnd, SW_SHOWDEFAULT);	
}

void Window::ClearEvent() {
	event = Event {};
}

void Window::Destroy() {
	DestroyWindow(hWnd);
}

void Window::CleanUp() {
	
	if (renderTarget) {
		renderTarget->Release();
		renderTarget = nullptr;
	}
	
	if (deviceContext) {
		deviceContext->Release();
		deviceContext = nullptr;
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

void Window::PostFileChangedEvent(FileChangedEvent* fileChangedEvent) {
	const auto lparam = reinterpret_cast<LPARAM>(fileChangedEvent);
	PostMessageA(hWnd, wmUserFileChanged, 0, lparam);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static f32 GetXFromLParam(LPARAM lParam) {
	return static_cast<f32>(LOWORD(lParam));
}

static f32 GetYFromLParam(LPARAM lParam) {
	return static_cast<f32>(HIWORD(lParam));
}

static u32 CollectKeyModifiers() {
	return 
		((HIWORD(GetKeyState(VK_CONTROL)) != 0) ? KM_Ctrl  : 0) |
		((HIWORD(GetKeyState(VK_SHIFT)) != 0)   ? KM_Shift : 0) |
		((HIWORD(GetKeyState(VK_MENU)) != 0)    ? KM_Alt   : 0);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LRESULT __stdcall WindowProc(HWND hWnd, UINT nMSG, WPARAM wParam, LPARAM lParam) {
	Window* self = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	
	if (!self)
		return DefWindowProc(hWnd, nMSG, wParam, lParam);

	switch (nMSG) {

		case WM_MOUSEMOVE: {
			mouse.x = GetXFromLParam(lParam);
			mouse.y = GetYFromLParam(lParam);
			return 0l;
		} break;
		
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP: {
			const f32 x = GetXFromLParam(lParam);
			const f32 y = GetYFromLParam(lParam);
			mouse.x = x;
			mouse.y = y;
			self->event.mouse.x = x;
			self->event.mouse.y = y;
			
			if (nMSG == WM_LBUTTONDOWN) {
				mouse.isDown = true;
				self->event.type = Event::Type_MouseDown;
			
			} else if (nMSG == WM_LBUTTONUP) {
				mouse.isDown = false;
				self->event.type = Event::Type_MouseUp;
			}
			
			return 0l;	
		} break;
		
		case WM_MOUSEWHEEL: {
			const int wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			const f32 distance = static_cast<f32>(wheelDelta) / WHEEL_DELTA;
			self->event.type = Event::Type_MouseWheel;
			self->event.wheelDistance = distance;
			return 0l;
		} break;
			
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			self->event.type = Event::Type_KeyPress;
			self->event.keypress.vkc = static_cast<u32>(wParam);
			self->event.keypress.mods = CollectKeyModifiers();
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
			
			// ascii-control character block 
			if (wch >= L'\0' && wch <= L'\x1f') break;
			
			// delete
			if (wch == L'\x7f') break;
			
			// Ctrl+Space is used for autocomplete. We need to supress this space
			if (wch == L' ' && HIWORD(GetKeyState(VK_CONTROL))) break;
			
			// @FIXME we need to use the Unicode version of RegisterClass (RegisterClassW) in order to recieve utf16 chars
			// the A-Variant just gives us characters in the current code-page
			
			if (!ToUtf8(std::wstring_view {&wch, 1u}, self->event.text.data, &self->event.text.len)) {
				LogWarning("failed to convert input to utf-8");
				return false;
			}
		
			self->event.type = Event::Type_Text;
			return 0l;
		} break;

		case WM_SIZE: {
			const u32 uwidth  = LOWORD(lParam);
			const u32 uheight = HIWORD(lParam);
			
			const f32 width  = static_cast<f32>(uwidth);
			const f32 height = static_cast<f32>(uheight);

			self->width = width;
			self->height = height;
			self->renderTarget->Resize(D2D_SIZE_U {uwidth, uheight});
			
			self->event.type = Event::Type_Resize;
			self->event.newSize.w = width;
			self->event.newSize.h = height;
			return 0l;
		} break;
		
		case WM_CLOSE: {
			self->event.type = Event::Type_Close;
			return 0l;
		} break;
		
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0l;
		} break;
		
		case WM_QUIT: {
			self->quitReceived = true;
			return 0l;
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
	
	} else if (nMSG == wmUserFileChanged) {
		self->event.type = Event::Type_FileChange;
		self->event.fileChangedEvent = reinterpret_cast<FileChangedEvent*>(lParam);
	} 
	
	return DefWindowProc(hWnd, nMSG, wParam, lParam);
}
