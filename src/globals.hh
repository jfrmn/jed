#pragma once
#include "basic.hh"

struct ID2D1DeviceContext;

// elpsed ms since last update
// is 0 if we went to sleep the last frame
extern f32 deltaTime;

// device context of the main window
extern ID2D1DeviceContext* deviceContext;

// flag that indicates that we need to keep the update loop running
// for example because an animation hasn't finished
extern bool needsUpdate;

