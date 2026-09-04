#include "app.hh"
#include "settings.hh"

#include "graphics.hh"
#include "glyph-run.hh"

#include "language/language.hh"
#include "language/json-helper.hh"

#include "ui/window.hh"
#include "ui/animation.hh"
#include "logging.hh"
#include "file-watcher.hh"



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// NOTE: don't make these functions static, they are used in the test-executable also...

bool Init() {
	if (!InitJsonLib()) {
		LogFatal("failed to set malloc and free for json allocator");
		return false;
	}
	
	if (!InitFactories()) {
		LogFatal("init directx factories failed");
		return false;
	}
	
	if (!InitStaticShapingBuffer()) {
		LogFatal("init StaticTextAnalyzer failed");
		return false;
	}
	
	if (!InitPerformanceCounter()) {
		LogFatal("init performance counter failed");
		return false;
	}

	const Window::CreateParams windowCreateParams {
	 	.className = "JEDWND",
#ifndef _TESTING
	 	.title = "jed",
	 	.width = 1920,
	 	.height = 1080,
#else
	 	.title = "jed - TESTING",
	 	.width = 800,
	 	.height = 600,
#endif
	 	.hWndParent = NULL};
	
	if (!mainWindow.Create(windowCreateParams)) {
		LogFatal("creating window failed");
		return false;
	}
	
	if (!settings.Init(mainWindow.deviceContext)) {
		LogWarning("failed to load settings");
	}
	
	if (!InitGraphics(mainWindow.deviceContext)) {
		LogFatal("init effects failed");
		return false;
	}
	
	if (!Language::LoadLanguages(".\\config\\languages"))
		LogError("failed to load languages");
	
	if (!fileWatcher.Init()) {
		LogFatal("init file watcher failed");
		return false;
	}
		
	if (!app.Init()) {
		LogError("init main window failed");
		return false;
	}
	
	return true;
}

void Shutdown() {
	app.Shutdown();
	mainWindow.CleanUp();
	fileWatcher.Shutdown();
	ShutdownGraphics();
	ShutdownFactories();
	CloseLogger();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifndef _TESTING

int main(int argc, char** argv) {
	OpenLogger(LogLevel_Trace, LogOutput_Stdout);

	LogInfo("application startup");
	if (!Init()) return -1;
	
	LogInfo("showing window");
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
	
	Shutdown();
	return 0;
}
#endif