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
bool Mouse::Element::operator==(const Element& other) const {
	return userdata == other.userdata
		&& onClickFunc == other.onClickFunc;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool Mouse::Hittest(const D2D_RECT_F& area, void* userdata, Mouse::OnClickFunction onClick, u64 arg /*= 0*/) {
	
	const Element newElement {userdata, onClick};
	
	if (isDragging)
		return hotElement == newElement;
	
	if (RectContains(area, x, y)) {
		hotElementNext = newElement;
		onClickArg = arg;
		return hotElement == newElement;
	}
	
	return false;
}

void Mouse::StartDragging(f32 arg /*= 0.0f*/) {
	isDragging = true;
	dragArg = arg;
	
	// not sure if this is needed?
	hotElementNext = {}; 
	onClickArg = 0u;
}

void Mouse::NextFrame() {

	if (isDragging) {
		if (event == Event_Up) {
			dragArg = 0.0f;
			isDragging = false;
		}

	} else {
		hotElement = hotElementNext;
		hotElementNext = {};

		if (event == Event_Up) {
			if (hotElement.onClickFunc)
				hotElement.onClickFunc(hotElement.userdata, onClickArg);
		}
	}
	
	event = Event_None;
}
