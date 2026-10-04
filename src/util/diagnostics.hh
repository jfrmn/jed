#pragma once

union Color;
struct ID2D1Bitmap;
struct ID2D1SolidColorBrush;

namespace Diagnostics {
	
	enum Severity {
		 Severity_Unknown = 0,
		 Severity_Error,
		 Severity_Warning,
		 Severity_Info,
		 Severity_Hint,
		 Severity_COUNT
	};
	
	// color for each severity
	extern const Color COLORS[Severity_COUNT];
	
	// index to the icon of each severity
	extern const int ICONS[Severity_COUNT];
	
	ID2D1SolidColorBrush* GetServerityBrush(Severity sev);
};