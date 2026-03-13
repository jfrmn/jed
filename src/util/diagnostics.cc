#include "diagnostics.hh"
#include "basic.hh"
#include "theme.hh"

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
	0l,
	static_cast<u32>(theme.icons.editorDiagnosticsError   - theme.iconArray[0]),
	static_cast<u32>(theme.icons.editorDiagnosticsWarning - theme.iconArray[0]),
	static_cast<u32>(theme.icons.editorDiagnosticsInfo    - theme.iconArray[0]),
	static_cast<u32>(theme.icons.editorDiagnosticsHint    - theme.iconArray[0])};

static_assert(STATIC_ARRAY_SIZE(Diagnostics::SEVERITY_ICON_INDICIES) == Diagnostics::Severity_MAX);