#include "globals.hh"
#include "events.hh"
#include "main-window.hh"
#include "file-watcher.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

Mouse mouse {};
f32 deltaTime = 0.0f;
ID2D1DeviceContext* deviceContext = nullptr;
bool needsUpdate = false;
MainWindow mainWindow {};
FileWatcher fileWatcher {};
