#pragma once

union Color;
struct ID2D1Bitmap;

namespace Diagnostics {
	
	enum Severity {
		 Severity_Unknown = 0,
		 Severity_Error,
		 Severity_Warning,
		 Severity_Info,
		 Severity_Hint,
		 Severity_MAX
	};
	
	// color for each severity
	extern const Color SEVERITY_COLORS[Severity_MAX];
	
	// index to the icon in the style.icons-array
	extern ID2D1Bitmap** SEVERITY_ICONS[];
};