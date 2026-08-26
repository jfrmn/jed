#pragma once
#include "basic.hh"
#include "commands.hh"
#include "ui/text-box.hh"

#include <vector>
#include <string>

struct ID2D1SolidColorBrush;
struct Event;
struct Command;

struct Explorer {
	
	//-------------------------------------------
	// types
	
	struct Panel;

	struct Item {
		enum Type {
			 Type_Unknown = 0,
			 Type_Directory = 1,
			 Type_File = 2,
			 Type_Placeholder = 3
		};
		
		enum Flag {
			 Flag_None     = 0,
			 Flag_Cut      = 1,
			 Flag_Inserted = 2,
			 Flag_Copied   = 4
		};
		
		Type type = Type_Unknown;
		
		
		bool isSelected = false;
		u32 flags = Flag_None;		
		
		std::string filename = {};
	};
		
	struct Panel {
		D2D_RECT_F area = {};
		Panel* parent = nullptr;
		
		std::string directoryName = {};
		u64 fullPathLength = 0u;
		
		std::vector<Item> items = {};
		Item* activeItem = nullptr;
	};

	struct NewItemDialog {
		D2D_RECT_F area = {};
		TextBox textbox = {};
		
		Item::Type itemType = Item::Type_Unknown;
		bool isRename = false;
		std::string errorText = {};
	};

	//-------------------------------------------
	// data

	Panel* activePanel = nullptr;
	NewItemDialog* newItemDialog = nullptr;
	
	bool insertAnimationRunning = false;
	f32  insertAnimationValue = 0.0f;
	
	bool copyAnimationRunning = false;
	f32  copyAnimationValue = 0.0f;
	
	f32 activeItemAnimationValue = 0.0f;
	
	bool shouldClose = false;
	
	//-------------------------------------------
	// functions
	
	static Explorer* Make();
	~Explorer() noexcept;
	
	void Update();	
	bool HandleEvent(const Event& event, const Command& commaand);
};