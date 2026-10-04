#pragma once
#include "basic.hh"
#include "editor/editor-toolwindow.hh"
#include "glyph-run.hh"
#include "util/diagnostics.hh"

#include <vector>

struct Editor;

struct EditorDiagnosticsList : public EditorToolWindow {
	
	//-----------------------------------------
	// types
	//-----------------------------------------
	
	struct Item {
		GlyphRun code = {};
		GlyphRunMultiline message = {};
		Diagnostics::Severity severity = Diagnostics::Severity_Unknown;
		f32 width = 0.0f;
		f32 height = 0.0f;
	};
	
	//-----------------------------------------
	// data
	//-----------------------------------------
	
	Editor* owner = nullptr;
	
	u64 selectedItem = 0u;
	std::vector<Item> items = {};
	
	s32 diagnosticsVersion = 0;
	f32 itemHighlightAnimationValue = .0f;
	
	//-----------------------------------------
	// functions
	//-----------------------------------------

	static EditorDiagnosticsList* Make(Editor* editor);
	
	virtual void Update() override;
	virtual bool HandleEvent(const Event& event) override;
	
	virtual bool IsDiagnosticsList() const override;
};
