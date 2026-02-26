#include "events.hh"
#include "util/rect-util.hh"

bool KeyEvent::operator==(KeyEvent other) const {
	return this->vkeycode == other.vkeycode
		&& this->ctrl     == other.ctrl
		&& this->shift    == other.shift
		&& this->alt      == other.alt;
}

bool KeyEvent::NoModifiers() const {
	return !ctrl && !shift && !alt;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static void ResetHotElementData(Mouse* self) {
	self->onClickFunc = nullptr,
	self->onClickUserdata = 0u;
}

bool Mouse::Hittest(const D2D_RECT_F& area, void* element, Mouse::OnClickFunction onClick, u64 userdata) {
	
	if (draggingElement)
		return element == draggingElement;
	
	if (RectContains(area, x, y)) {
		hotElementNext = element;
		onClickFunc = onClick,
		onClickUserdata = userdata;
		return hotElement == element;
	}
	
	return false;
}

void Mouse::StartDragging() {
	dragStartX = x;
	dragStartY = y;
	draggingElement = hotElement;
	
	// not sure if this is needed?
	hotElementNext = nullptr; 
	onClickFunc = nullptr;
	onClickUserdata = 0u;
}

bool Mouse::IsDragging() const {
	return draggingElement != nullptr;
}

void Mouse::NextFrame() {
	
	hotElement = hotElementNext;
	hotElementNext = nullptr;
	
	if (event == Event_Up) {
		if (draggingElement) {
			dragStartX = 0.0f;
			dragStartY = 0.0f;
			draggingElement = nullptr;
		
		} else {
			if (onClickFunc)
				onClickFunc(hotElement, onClickUserdata);
		}
	}
	event = Event_None;
}
