#pragma once
#include "editor/editor-toolwindow.hh"
#include "ui/text-box.hh"

struct Editor;
struct Event;

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

	virtual void Update() override;
	virtual bool HandleEvent(const Event& event) override;
	
	virtual bool IsGotoLine() const override;
};