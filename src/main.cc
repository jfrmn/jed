#include "app.hh"
#include "globals.hh"
#include "settings.hh"

#include "graphics.hh"
#include "glyph-run.hh"

#include "language/language.hh"
#include "language/json-helper.hh"

#include "ui/window.hh"
#include "logging.hh"
#include "file-watcher.hh"
 
// @DUMMY
#include "editor/editor.hh"
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool GetPerformanceFrequency(u64* ticksPerMs) {
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq)) {
		LogError("QueryPerformanceFrequency() failed. Last Error: %s", StrLastErr(GetLastError()));
		return false;
	}
	
	*ticksPerMs = (freq.QuadPart / 1000);
	LogInfo("Performance Frequency: %zu/ms", *ticksPerMs);
	return true;
}

static u64 GetPerformanceTimestamp() {
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);	
	return ticks.QuadPart;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int main(int argc, char** argv) {

	LogInfo("application startup");

	if (!InitJsonLib()) {
		LogFatal("failed to set malloc and free for json allocator");
		return -1;
	}
	
	if (!InitFactories()) {
		LogFatal("init directx factories failed");
		return -1;
	}
	
	if (!InitStaticShapingBuffer()) {
		LogFatal("init StaticTextAnalyzer failed");
		return -1;
	}
	
	u64 ticksPerMs = 0u;	
	if (!GetPerformanceFrequency(&ticksPerMs))
		LogError("init perforamnce counters failed");
	
	
	const Window::CreateParams windowCreateParams {.className = "JEDWND", .title = "jed", .width = 1920, .height = 1080, .hWndParent = NULL};
	if (!mainWindow.Create(windowCreateParams)) {
		LogFatal("creating window failed");
		return -1;
	}
	
	if (!settings.Init(mainWindow.deviceContext)) {
		LogWarning("failed to load settings");
	}
	
	if (!InitGraphics(mainWindow.deviceContext)) {
		LogFatal("init effects failed");
		return -1;
	}
	
	if (!Language::LoadLanguages(".\\config\\languages"))
		LogError("failed to load languages");
	
	if (!fileWatcher.Init()) {
		LogFatal("init file watcher failed");
		return -1;
	}
		
	if (!app.Init()) {
		LogError("init main window failed");
		return -1;
	}
	
	mainWindow.Show();
		
	LogInfo("running message loop");

	u64 ticksBefore = 0;
	while (!mainWindow.quitReceived) {
		
		if (needsUpdate) {
			if (MSG message; PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
			
			const u64 ticksAfter = GetPerformanceTimestamp();
			
			deltaTime = static_cast<f32>(ticksAfter - ticksBefore) / ticksPerMs;
			ticksBefore = ticksAfter;
			
		} else {
			MSG message;
			GetMessage(&message, NULL, 0, 0);
			TranslateMessage(&message);
			DispatchMessage(&message);
			
			deltaTime = 0.0f;
			ticksBefore = GetPerformanceTimestamp();
		}
		
		needsUpdate = false;		
		app.PullEvent();
		app.Update();
		mouse.NextFrame(mainWindow.event);
		mainWindow.ClearEvent();
	}
	
	app.Shutdown();
	mainWindow.CleanUp();
	fileWatcher.Shutdown();
	ShutdownGraphics();
	ShutdownFactories();	
	return 0;
}