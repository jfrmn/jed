#pragma once
#include "basic.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d2d1.h>

struct Scrollarea {

	//---------------------------------------------------------
	// data

	// position of the bar
	D2D_POINT_2F position = {};

	// viewport
	f32 vpX = 0.0f;
	f32 vpY = 0.0f;
	D2D_SIZE_F vpSize = {};

	// total size
	D2D_SIZE_F totalSize = {};
	
	// width of the bar that gets rendered
	f32 barWidth = 0.0f;
		
	//---------------------------------------------------------
	// funcitons

	void Init(f32 posX, f32 posY, f32 width, f32 height, f32 barWidth);
	void ResetViewport();
		
	void ScrollVertical(f32 amount);
	float GetMaxPositionX() const;
	float GetMaxPositionY() const;

	void DrawMarker(ID2D1RenderTarget* renderTarget, ID2D1Brush* brush, float absoluteVerticalPosition);
	f32 GetRatio();

	void OnUpdate();
	void OnResize(D2D_RECT_F hostRect);
};