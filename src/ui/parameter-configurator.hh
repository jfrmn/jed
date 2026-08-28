#pragma once
#include "basic.hh"
#include "ui/text-box.hh"
#include "util/parameter.hh"

#include <span>

struct ParameterConfigurator {
	
	enum Result {
		 Result_Unfinished = 0,
		 Result_Run = 1,
		 Result_Cancel = -1
	};
	
	struct Item {
		const Parameter* parameter = nullptr;		
		TextBox textBox = {};
		u64 selectedEnumIndex;
		bool isChecked;
	
		bool HasTextBox() const;
	};
	
	//-----------------------------------------------------
	// data
	
	D2D_RECT_F area = {};
	
	Item* items = nullptr;
	u64 itemCount = 0u;
	u64 selectedItem = 0u;
	
	bool isDropDownOpen = false;
	bool isCancelButtonSelected = false;
	
	Result result = Result_Unfinished;
	
	//-----------------------------------------------------
	// functions
	
	static ParameterConfigurator* Make(std::span<const Parameter> paramters, const std::vector<ParameterValue>* initialValues = nullptr);
	~ParameterConfigurator() noexcept;
	
	void GetParameterValues(std::vector<ParameterValue>* initialValues) const;
	
	void Update();
	void OnResize();
	
	bool HandleEvent(const Event& event);
};
