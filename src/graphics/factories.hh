#pragma once

struct ID2D1Factory;
struct IDWriteFactory;
struct IWICImagingFactory;

extern ID2D1Factory* d2dFactory;
extern IDWriteFactory* dwFactory;
extern IWICImagingFactory* wicFactory;

bool InitFactories();
void ShutdownFactories();