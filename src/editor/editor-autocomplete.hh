#pragma once
#include "editor/editor-caretattached.hh"
#include "text/text-position.hh"
#include "util.hh"

#include <string>

struct Editor;
struct EditorSignatureHelp;
struct KeyEvent;
struct ID2D1DeviceContext;
struct ID2D1Bitmap;

struct EditorAutocomplete : public EditorCaretAttached  {

	//-------------------------------------------
	// types

	struct Item {
		enum Type {
		 	 Type_Unknown = 0,
		 	 Type_Text,
		 	 Type_Method,
		 	 Type_Function,
		 	 Type_Constructor,
		 	 Type_Field,
		 	 Type_Variable,
		 	 Type_Class,
		 	 Type_Interface,
		 	 Type_Module,
		 	 Type_Property,
		 	 Type_Unit,
		 	 Type_Value,
		 	 Type_Enum,
		 	 Type_Keyword,
		 	 Type_Snippet,
		 	 Type_Color,
		 	 Type_File,
		 	 Type_Reference,
		 	 Type_Folder,
		 	 Type_EnumMember,
		 	 Type_Constant,
		 	 Type_Struct,
		 	 Type_Event,
		 	 Type_Operator,
		 	 Type_TypeParameter,
		 	 Type_MAX
		};
		
		Type type = Type_Unknown;
		std::string label = {};
		std::string details = {};
		std::string documentation = {};
		
		TextPosition insertPosition = {};
		std::string insertText = {};
		
		bool matched = false;
		FuzzyMatchResult matchResult = {};
		
		f32 clandScrore = 0.0f;
	};
	
	//-------------------------------------------
	// data
	
	Item* items = nullptr;
	u64 itemCount = 0u;
	u64 selectedItem = U64_MAX;
	
	EditorSignatureHelp* signatureHelp = nullptr;
	
	//-------------------------------------------
	// functions
	
	static EditorAutocomplete* Make(Editor* editor);
	
	void SortItems();
	
	virtual void Update() override;
	virtual void OnInput() override;
	virtual bool HandleEvent(const Event& event, const Command& command) override;
	
	virtual ~EditorAutocomplete() noexcept;
};