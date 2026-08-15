#pragma once
#include "basic.hh"
#include "editor/editor-toolwindow.hh"
#include "ui/text-box.hh"

struct Editor;
struct KeyEvent;
struct ID2D1DeviceContext;

struct EditorGotoLine : public EditorToolWindow {

	//-----------------------------------------------------
	// data

	Editor* owner = nullptr;
	D2D_RECT_F area = {};

	TextBox textbox = {};

	GlyphRun glyphRunHeadline = {};

	//-----------------------------------------------------
	// functions

	static EditorGotoLine* Make(Editor *editor);

	virtual void OnUpdate() override;
	virtual void OnKeyEvent(KeyEvent event, Command command) override;
	virtual void OnChar(const char* data, u64 len) override;
	
	virtual bool IsGotoLine() const override;
};