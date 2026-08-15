#pragma once
#include "basic.hh"
#include "editor/editor-toolwindow.hh"

struct Editor;
struct KeyEvent;

struct EditorDiagnosticsList : public EditorToolWindow {
	
	//-----------------------------------------
	// data
	
	Editor* owner = nullptr;
	u64 selectedItem = 0u;
	u64 itemCount = 0u;
	
	f32 itemHighlightAnimationValue = .0f;
	
	//-----------------------------------------
	// functions

	static EditorDiagnosticsList* Make(Editor* editor);
	
	virtual void OnUpdate() override;
	virtual void OnKeyEvent(KeyEvent event, Command command) override;
	
	virtual bool IsDiagnosticsList() const override;
};
