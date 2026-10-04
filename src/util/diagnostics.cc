#include "diagnostics.hh"
#include "basic.hh"
#include "settings.hh"
#include "graphics.hh"
#include "ui/icons.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

const Color Diagnostics::COLORS[] {
	Color::FromKnown(D2D1::ColorF::White),
	Color::FromKnown(D2D1::ColorF::Red),
	Color::FromKnown(D2D1::ColorF::Yellow),
	Color::FromKnown(D2D1::ColorF::Green),
	Color::FromKnown(D2D1::ColorF::LightBlue)};
	
static_assert(STATIC_ARRAY_SIZE(Diagnostics::COLORS) == Diagnostics::Severity_COUNT);

const int Diagnostics::ICONS[] {
	ICON_UNKNOWN,
	ICON_EDITORDIAGNOSTICS_ERROR,
	ICON_EDITORDIAGNOSTICS_WARNING,
	ICON_EDITORDIAGNOSTICS_INFO,
	ICON_EDITORDIAGNOSTICS_HINT};

static_assert(STATIC_ARRAY_SIZE(Diagnostics::ICONS) == Diagnostics::Severity_COUNT);

ID2D1SolidColorBrush* Diagnostics::GetServerityBrush(Diagnostics::Severity sev) {
	brush->SetColor(COLORS[sev].ToD2D());
	return brush;
}
