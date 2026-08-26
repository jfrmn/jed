#include "events.hh"
#include "util.hh"

MouseState mouse {};

std::string_view Event::GetText() const {
	return std::string_view {textData, textLen};
}

bool MouseState::Hittest(const D2D_RECT_F& area, void* userdata, MouseState::Callback callback /*= nullptr*/, u64 userint /*= 0*/) {
	if (isDragging) return false;
	
	const Element newElement {callback, userdata, userint};
	
	if (RectContains(area, x, y)) {
		nextHotElement = newElement;
		return currentHotElement == newElement;
	}
	
	return false;
}

bool MouseState::Hot(void* userdata, MouseState::Callback callback /*= nullptr*/, u64 userint /*= 0*/) {
	if (isDragging) return false;
	
	const Element newElement {callback, userdata, userint};
	nextHotElement = newElement;
	return currentHotElement == newElement;
}

void MouseState::StartDragging(f32 dx /*= 0.0f*/, f32 dy /*= 0.0f*/) {
	isDragging = true;
	dragDeltaX = dx;
	dragDeltaY = dy;
	nextHotElement = {};
}

void MouseState::NextFrame(const Event& event) {

	if (event.type == Event::Type_MouseUp) {
		if (isDragging) {
			isDragging = false;
			dragDeltaX = dragDeltaY = 0.0f;
		} else if (currentHotElement.callback) {
			currentHotElement.callback(currentHotElement.userdata, currentHotElement.userint);
		}
	}

	if (!isDragging) {
		currentHotElement = nextHotElement;
		nextHotElement = Element {};
	}
}
