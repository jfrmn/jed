#pragma once
#include <vector>
#include "text/text-position.hh"
#include "graphics/glyph-run.hh"
#include "commands/parameter.hh"

#include "ui/scrollarea.hh"
#include "ui/file-preview.hh"

#include "util/process.hh"
#include "util/regex.hh"
#include "util/color.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

#include <mutex>

struct GlyphRun;
struct Tool;
struct KeyEvent;

struct Console : public Process::Observer {
	
	//------------------------------------------
	// types
	//------------------------------------------
	
	enum StyleChangeType {
		 StyleChangeType_Unknown = 0,
		 StyleChangeType_Bold,
		 StyleChangeType_Underline,
		 StyleChangeType_Negative,
		 StyleChangeType_Foreground,
		 StyleChangeType_ForegroundDefault,
		 StyleChangeType_Background,
		 StyleChangeType_BackgroundDefault,
		 StyleChangeType_Reset = 255
	};
	
	struct StyleChange {
		StyleChangeType type = StyleChangeType_Reset;
		TextPosition position = {};
		Color color = {};
		bool value = false;
	};
		
	struct ToolDiagnosticsRecord {
		std::string message = {};
		std::string_view source = {};
		u64 position = 0u;
	};
	
	struct EditorDiagnosticsRecord {
		std::string file = {};
		u64 line = 0u;
		Color color = {};
		
		u64 originLine = 0u;
		u64 originFromColumn = 0u;
		u64 originToColumn = 0u;
	};
	
	//------------------------------------------
	// data
	//------------------------------------------
	
	D2D_RECT_F area = {};
	bool isOpen = false;
	
	std::mutex mtx = {};
	
	const Tool* tool = nullptr;
	std::vector<ParameterValue> toolParameterValues = {};
	
	// anything that wen't wrong with the tool itself
	// not to be confused with diagnosticsRecords which contains the matched records
	std::vector<ToolDiagnosticsRecord> toolDiagnostics = {};
	bool showToolDiagnostics = false;
	
	f32         progressValue = 0.0f;
	std::string progressText = {};
	
	std::vector<EditorDiagnosticsRecord> diagnosticsRecords = {};
	u64         selectedDiagnosticsRecord = 0;
	FilePreview filePreview = {};
	
	Process*    process = nullptr;
	
	std::vector<StyleChange> styleChanges = {};
	std::vector<std::string> lines = {};
	
	bool                  glyphRunCacheIsValid = false;
	std::vector<GlyphRun> glyphRunCache = {};
	
	bool       disableAutoScroll = false;
	Scrollarea scrollarea = {};
	
	TextPosition selectionStart = {};
	TextPosition selectionEnd = {};
	
	//------------------------------------------
	// functions
	//------------------------------------------
	
	bool Init();
	
	bool StartProcess();
	void OnUpdate();
	
	void OnResize(f32 newWidth, f32 newHeight);
	void OnMouseWheel(f32 distance);
	bool OnKeyDown(KeyEvent event);
	
	virtual void OnStderr(std::string_view data) override;
	virtual void OnStdout(std::string_view data) override;
	
	virtual void OnStarted() override;
	virtual void OnExited(int exitCode) override; 
};
