#pragma once
#include "editor-caretattached.hh"
#include "language/language.hh"
#include <basic.hh>
#include <string_view>

struct Editor;
struct KeyEvent;

struct EditorSelectGotoType : public EditorCaretAttached {
	
	struct Item {
		std::string_view label = {};
		EditorTextLocationList* (Language::*GotoFunction)(Editor*) = nullptr;
	};
	
	u64 selectedItem = 0u;
	
	static EditorSelectGotoType* Make(Editor* owner);	
	
	virtual void OnUpdate() override;
	virtual bool OnKeyDown(KeyEvent event) override;
	virtual void OnInput() override;
	
	virtual ~EditorSelectGotoType() {}
};
