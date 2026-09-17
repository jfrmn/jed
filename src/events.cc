#include "events.hh"
#include "util.hh"

MouseState mouse {};

bool MouseState::Hittest(const D2D_RECT_F& area, void* userdata, MouseState::Callback callback /*= nullptr*/, u64 userint /*= 0*/) {
	
	const HotZone newHotZone {callback, userdata, userint};	
	if (RectContains(area, x, y)) {
		nextHotZone = newHotZone;
		return currHotZone == newHotZone;
	}
	
	return false;
}

bool MouseState::Hot(void* userdata, MouseState::Callback callback /*= nullptr*/, u64 userint /*= 0*/) {
	const HotZone newHotZone {callback, userdata, userint};
	nextHotZone = newHotZone;
	return currHotZone == newHotZone;
}

void MouseState::StartDragging(f32 dx /*= 0.0f*/, f32 dy /*= 0.0f*/) {
	isDragging = true;
	dragDeltaX = dx;
	dragDeltaY = dy;
	nextHotZone = {};
}

bool MouseState::IsDragging(void* userdata, u64 userint /*= 0u*/) {
	const HotZone hotZone {nullptr, userdata, userint};
	return isDragging && (currHotZone == hotZone);
}

void MouseState::NextFrame(const Event& event) {

	if (event.type == Event::Type_MouseUp) {
		if (isDragging) {
			isDragging = false;
			dragDeltaX = dragDeltaY = 0.0f;
		} else if (currHotZone.onClick) {
			currHotZone.onClick(currHotZone.userdata, currHotZone.userint);
		}
	}

	if (!isDragging) {
		currHotZone = nextHotZone;
		nextHotZone = HotZone {};
	}
}
