#pragma once
#include "editor-caretattached.hh"
#include "ui/file-preview.hh"

struct EditorTextLocationList : public EditorCaretAttached {
	
	//-------------------------------------------
	// types
	
	struct Range {
		TextPosition start = {};
		TextPosition end = {};
	};
	
	struct Item {
		std::string targetPath = {};
		std::string_view filename = {};
		
		Range selectionRange = {};
	};
	
	struct ItemEx : public Item {
		Range fullTargetRange = {};
		Range originSelectionRange = {};
	};

	//-------------------------------------------
	// data

	Item* items = nullptr;
	u64 itemCount = 0u;
	u64 selectedItem = U64_MAX;
	bool extendedItems = false;
	
	bool completed = false;
	std::string error = {};
	
	FilePreview filePreview = {};

	//-------------------------------------------
	// functions
	
	static EditorTextLocationList* Make(Editor* owner);
	
	virtual void Update() override;
	virtual bool HandleEvent(const Event& event) override;
	virtual void OnInput() override;
	
	void UpdateFilePreview();
	
	virtual ~EditorTextLocationList();
};
