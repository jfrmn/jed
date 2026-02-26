#include "diagnostics.hh"
#include "basic.hh"
#include "ui/style.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>


const _D3DCOLORVALUE Diagnostics::SEVERITY_COLORS[] {
	D2D1::ColorF(D2D1::ColorF::White),
	D2D1::ColorF(D2D1::ColorF::Red),
	D2D1::ColorF(D2D1::ColorF::Yellow),
	D2D1::ColorF(D2D1::ColorF::Green),
	D2D1::ColorF(D2D1::ColorF::LightBlue)};
	
static_assert(STATIC_ARRAY_SIZE(Diagnostics::SEVERITY_COLORS) == Diagnostics::Severity_MAX);

const unsigned long Diagnostics::SEVERITY_ICON_INDICIES[] {
	Style::Icon_Unknown,
	Style::Icon_EditorDiagnostics_Error,
	Style::Icon_EditorDiagnostics_Warning,
	Style::Icon_EditorDiagnostics_Info,
	Style::Icon_EditorDiagnostics_Hint};

static_assert(STATIC_ARRAY_SIZE(Diagnostics::SEVERITY_ICON_INDICIES) == Diagnostics::Severity_MAX);