#include "scrollarea.hh"
#include "globals.hh"
#include "events.hh"
#include "settings.hh"
#include "util.hh"
#include "logging.hh"

#include <algorithm>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

void Scrollarea::Init(f32 posX, f32 posY, f32 width, f32 height, f32 barWidth) {
	vpX = vpY = 0.0f;
	this->vpSize    = D2D_SIZE_F   {width, height};
	this->position  = D2D_POINT_2F {posX,  posY};
	this->totalSize = D2D_SIZE_F   {width, height};
	this->barWidth = barWidth;
}

void Scrollarea::ResetViewport() {
	vpX = vpY = 0.0f;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Scrollarea::ScrollVertical(float amount) {
	// NOTE do not use clamp here
	// std::clamp() has an assert that min must be < max
	// this is not necessarily true in our case

	const f32 newViewportY = vpY - amount;
	const f32 maxViewportPos = totalSize.height - vpSize.height;
	
	if (newViewportY < .0f || maxViewportPos < .0f) {
		vpY = .0f;
		return;
	}
	
	if (newViewportY > maxViewportPos) {
		vpY = maxViewportPos;
		return;
	}
	
	vpY = newViewportY;
}

f32 Scrollarea::GetMaxPositionX() const {
	return std::max(0.0f, totalSize.width - vpSize.width);
}

f32 Scrollarea::GetMaxPositionY() const {
	return std::max(0.0f, totalSize.height - vpSize.height);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void Scrollarea::OnUpdate() {
	
	const f32 ratio = vpSize.height / totalSize.height;
	if (ratio >= 1.0f) return;

	const D2D_RECT_F vertBar {
		.left   = position.x + vpSize.width - barWidth,
		.top    = position.y + (vpY * ratio),
		.right  = position.x + vpSize.width,
		.bottom = position.y + ((vpY + vpSize.height) * ratio)};
	
	deviceContext->FillRectangle(vertBar, settings.GetBrushUiBackground());

	if (mouse.Hittest(vertBar, this, nullptr)) {
		if (mouse.event == Mouse::Event_Down)
			mouse.StartDragging(mouse.y - vertBar.top);
		
		deviceContext->FillRectangle(vertBar, settings.GetBrushHover(mouse.isDragging));
		
		if (mouse.isDragging) {
			const f32 newVpY = -(mouse.dragArg - mouse.y + position.y) / ratio;
			const f32 max = GetMaxPositionY();
			//LogDevVar(mouse.dragArg);
			
			vpY = std::clamp(newVpY, 0.0f, max);
			//LogDev("%d - %d - %d", mouse.dragArg, (int)newVpY, (int)vpY);
				
		}
	}
}

void Scrollarea::OnResize(D2D_RECT_F hostRect) {
	position = D2D_POINT_2F {hostRect.left, hostRect.top};
	vpSize = RectSize(hostRect);
}

f32 Scrollarea::GetRatio() {
	const f32 ratio = vpSize.height / totalSize.height;
	return ratio < 1.0f ? ratio : 1.0f;
}