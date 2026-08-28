#pragma once
#include "editor-caretattached.hh"
#include "language/language.hh"
#include <basic.hh>
#include <string_view>

struct Editor;

struct EditorSelectGotoType : public EditorCaretAttached {
	
	struct Item {
		std::string_view label = {};
		EditorTextLocationList* (Language::*GotoFunction)(Editor*) = nullptr;
	};
	
	u64 selectedItem = 0u;
	
	static EditorSelectGotoType* Make(Editor* owner);	
	
	virtual void Update() override;
	virtual bool HandleEvent(const Event& event) override;
	virtual void OnInput() override;
	
	virtual ~EditorSelectGotoType() {}
};
