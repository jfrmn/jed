#include "main-window.hh"
#include "globals.hh"
#include "settings.hh"

#include "graphics/factories.hh"
#include "graphics/effects.hh"
#include "graphics/glyph-run.hh"

#include "language/language.hh"
#include "json/json-basic.hh"
#include "util/logging.hh"
#include "file-watcher.hh"
 
// @DUMMY
#include "editor/editor.hh"
 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool GetPerformanceFrequency(u64* ticksPerMs) {
	LARGE_INTEGER freq;
	if (!QueryPerformanceFrequency(&freq)) {
		LogError("QueryPerformanceFrequency() failed. Last Error: %", FLastErr(GetLastError()));
		return false;
	}
	
	*ticksPerMs = (freq.QuadPart / 1000);
	LogInfo("Performance Frequency: %/ms", *ticksPerMs);
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
	
	
	if (!mainWindow.Create()) {
		LogFatal("creating window failed");
		return -1;
	}
	
	if (!settings.Init(mainWindow.deviceContext)) {
		LogWarning("failed to load settings");
	}
	
	if (!InitEffects(mainWindow.deviceContext)) {
		LogFatal("init effects failed");
		return -1;
	}
	
	if (!Language::LoadLanguages(".\\config\\languages"))
		LogError("failed to load languages");
	
	if (!fileWatcher.Init()) {
		LogFatal("init file watcher failed");
		return -1;
	}
		
	if (!mainWindow.Init()) {
		LogError("init main window failed");
		return -1;
	}
	
	// @DUMMY
	mainWindow.OpenEditor(".\\src\\main.cc", MainWindow::OpenBehavior_NewPanelLeft);
	
	LogInfo("running message loop");

	u64 ticksBefore = 0;
	while (!mainWindow.destroyRecieved) {
		
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
		mainWindow.OnUpdate();
		mouse.NextFrame();
	}
	
	mainWindow.Shutdown();
	fileWatcher.Shutdown();
	ShutdownEffects();
	ShutdownFactories();	
	return 0;
}