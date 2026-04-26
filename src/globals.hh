#pragma once
#include "basic.hh"

struct ID2D1DeviceContext;
struct D2D_POINT_2F;
struct MainWindow;
struct Mouse;
struct FileWatcher;

// current mouse cursor location in client coordiantes
extern Mouse mouse;

// elpsed ms since last update
// is 0 if we went to sleep the last frame
extern f32 deltaTime;

// device context of the main window
extern ID2D1DeviceContext* deviceContext;

// flag that indicates that we need to keep the update loop running
// for example because an animation hasn't finished
extern bool needsUpdate;

// the main window
extern MainWindow mainWindow;

// the file watcher
extern FileWatcher fileWatcher;
