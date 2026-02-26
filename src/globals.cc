#include "globals.hh"
#include "main-window.hh"
#include "events.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

MainWindow mainWindow {};
Mouse mouse {};
f32 deltaTime = 0.0f;
ID2D1DeviceContext* deviceContext = nullptr;
bool needsUpdate = false;
u64 tickCounter = 0u;
u64 ticksToLive = 0u;
u64 ticksPerSecond = 0u;