#pragma once
#include "basic.hh"
#include "editor/editor-caretattached.hh"

#include <string>
#include <span>

struct Editor;
struct KeyEvent;
struct ID2D1DeviceContext;

struct EditorSignatureHelp : public EditorCaretAttached {
	
	//-------------------------------------------
	// types
		
	struct Parameter {
		std::string documentation = {};
		
		bool labelIsSubstring = false;
		union {
			u64 labelOffset; // offset to start in signature.label
			char* labelData; // allocated data
		};
		u64 labelLength;
	};
	
	struct Signature {
		std::string label = {};
		std::string documentation = {};
		std::span<Parameter> parameter = {};
		u64 activeParameter = U64_MAX;
	};
		
	//-------------------------------------------
	// data
	
	Parameter* parameters = nullptr;
	u64 parameterCount = 0u;
	
	Signature* signatures = nullptr;
	u64 signatureCount = 0u;
	
	u64 activeSignature = 0u;
	u64 activeParameter = U64_MAX;
	
	bool autocompleteIsActive = false;	
	
	//-------------------------------------------
	// functions
	
	static EditorSignatureHelp* Make(Editor* owner);
	
	virtual void OnUpdate() override;
	virtual bool OnKeyEvent(KeyEvent event) override;
	virtual void OnInput() override;
		
	virtual ~EditorSignatureHelp() noexcept;
};