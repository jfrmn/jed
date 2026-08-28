#include "diagnostics.hh"
#include "basic.hh"
#include "settings.hh"
#include "graphics.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

const Color Diagnostics::SEVERITY_COLORS[] {
	Color::FromKnown(D2D1::ColorF::White),
	Color::FromKnown(D2D1::ColorF::Red),
	Color::FromKnown(D2D1::ColorF::Yellow),
	Color::FromKnown(D2D1::ColorF::Green),
	Color::FromKnown(D2D1::ColorF::LightBlue)};
	
static_assert(STATIC_ARRAY_SIZE(Diagnostics::SEVERITY_COLORS) == Diagnostics::Severity_MAX);

ID2D1Bitmap** Diagnostics::SEVERITY_ICONS[]{
	&settings.icons.unknown,
	&settings.icons.editorDiagnosticsError,
	&settings.icons.editorDiagnosticsWarning,
	&settings.icons.editorDiagnosticsInfo,
	&settings.icons.editorDiagnosticsHint};

static_assert(STATIC_ARRAY_SIZE(Diagnostics::SEVERITY_ICONS) == Diagnostics::Severity_MAX);

ID2D1SolidColorBrush* Diagnostics::GetServerityBrush(Diagnostics::Severity sev) {
	brush->SetColor(SEVERITY_COLORS[sev].ToD2D());
	return brush;
}
