#include "parameter-configurator.hh"
#include "globals.hh"
#include "events.hh"
#include "main-window.hh"
#include "settings.hh"

#include "util/logging.hh"
#include "util/rect-util.hh"
#include "ui/constants.h"
#include "graphics/effects.hh"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ParameterConfigurator::Item::HasTextBox() const {
	return parameter->type == Parameter::Type_String || parameter->type == Parameter::Type_Number;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ParameterConfigurator* ParameterConfigurator::Make(std::span<const Parameter> parameters, const std::vector<ParameterValue>* initialValues /*= nullptr*/) {
	ASSERT(!initialValues || parameters.size() == initialValues->size());
	
	auto self = new ParameterConfigurator();
	self->itemCount = parameters.size();
	self->items = new Item[parameters.size()];

	bool ok = true;	
	for (u64 i = 0u; i < self->itemCount; i++) {
		Item& item = self->items[i];
		item.parameter = &parameters[i];
		
		const ParameterValue& defaultValue = initialValues
			? initialValues->at(i)
			: parameters[i].defaultValue;
		
		if (item.HasTextBox()) {			
			std::string initalValue {};
			if (item.parameter->type == Parameter::Type_String)
				initalValue = defaultValue.stringValue;
			else if (item.parameter->type == Parameter::Type_Number)
				initalValue = std::to_string(defaultValue.numberValue);
				
			if (!item.textBox.Init(&settings.fontUi, {}, std::move(initalValue))) {
				LogError("failed to init textbox for parameter #%", i);
				delete self;
				return nullptr;
			}
			
		} else if (item.parameter->type == Parameter::Type_Bool) {
			item.isChecked = defaultValue.boolValue;
		
		} else if (item.parameter->type == Parameter::Type_Enum) {
			item.selectedEnumIndex = defaultValue.enumIndex;
		}
	}
		
	self->OnResize();
	return self;
}

ParameterConfigurator::~ParameterConfigurator() noexcept {
	delete[] items;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static f32 GetItemHeight() {
	return settings.fontUi.lineHeight + PADDING_X4;
}

static void OnClickMinus(void* ud, u64 i) {
}
static void OnClickPlus(void* ud, u64 i) {
}

static bool CheckParameters(const ParameterConfigurator* self) {
	for (u64 i = 0u; i < self->itemCount; i++) {
		if (self->items[i].HasTextBox() && self->items[i].textBox.invalid)
			return false;
	}
	return true;
}

void ParameterConfigurator::OnUpdate() {
	
	//
	// draw background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, area);
		if (!background) return;
		DEFER(background->Release());
		
		DrawGlow(deviceContext, background, area);
		
		PushLayer(deviceContext, area);
		BlurArea(deviceContext, area, background);
		PopLayer(deviceContext);
	}
			
	//
	// draw run and cancel buttons
	//
	{
		const D2D_RECT_F areaButton[2] {
		 	D2D_RECT_F {
				.left   = area.left,
				.top    = area.bottom - PADDING_X2 - settings.fontUi.lineHeight,
				.right  = area.left + (RectWidth(area) / 2.0f),
				.bottom = area.bottom},
			D2D_RECT_F {
				.left   = area.left + (RectWidth(area) / 2.0f),
				.top    = area.bottom - PADDING_X2 - settings.fontUi.lineHeight,
				.right  = area.right,
				.bottom = area.bottom}};
		const std::string_view text[2] {"Cancel", "Run"};
		const Color  backgroundColors[2] {Color::FromKnown(D2D1::ColorF::Red), Color::FromKnown(D2D1::ColorF::Green)};
		const bool isButtonSelected[2] {isCancelButtonSelected, !isCancelButtonSelected};
		const bool isEnabled[2] {true, CheckParameters(this)};
		
		for (int i = 0; i < 2; i++) {
			
			if (selectedItem == itemCount && isButtonSelected[i]) {
				deviceContext->FillRectangle(
					areaButton[i],
					GetBrush(isEnabled[i] ? backgroundColors[i] : settings.colors.uiBackground));
			} else {
				deviceContext->FillRectangle(areaButton[i], settings.GetBrushUiBackground(false));
			}
			
			staticGlyphRun.Shape(text[i], settings.fontUi);
			staticGlyphRun.Draw(deviceContext,
				areaButton[i].left + (RectWidth(areaButton[i]) / 2.0f) - (staticGlyphRun.width / 2.0f),
				areaButton[i].top + PADDING,
				settings.fontUi,
				settings.GetBrushUiText(isEnabled[i]));
		}
	}
		
	GlyphRun run {};
	const f32 itemHeight = GetItemHeight();
	
	//
	// draw items
	//
	for (s64 i = itemCount - 1; i >= 0; i--) {
		Item& item = items[i];
		
		const D2D_RECT_F itemArea {
			.left   = area.left,
			.top    = area.top + (i * itemHeight),
			.right  = area.right,
			.bottom = area.top + ((i+1) * itemHeight)};
		
		run.Shape(item.parameter->name, settings.fontUi);
		run.Draw(deviceContext, itemArea.left + MARGIN, itemArea.top + PADDING_X2, settings.fontUi, settings.GetBrushUiText());
		
		const bool isSelected = (i == selectedItem);
		
		// draw textbox
		if (item.HasTextBox()) {
			item.textBox.inactive = !isSelected;
			item.textBox.OnUpdate();
			
			// draw plus and minus button next to the textbox
 			if (item.parameter->type == Parameter::Type_Number) {
	 			const f32 btnWidth = settings.fontUi.lineHeight + PADDING_X2;
	 			const D2D_RECT_F textBoxArea = item.textBox.GetArea();
	 			
	 			{
					const D2D1_ROUNDED_RECT areaMinusButton = MakeRoundedRect(
						D2D_RECT_F {
							.left = textBoxArea.left - btnWidth,
							.top = textBoxArea.top,
							.right = textBoxArea.left,
							.bottom = textBoxArea.bottom},
						RADIUS);
						
					deviceContext->DrawRoundedRectangle(areaMinusButton, settings.GetBrushUiText(isSelected));
					if (mouse.Hittest(areaMinusButton.rect, this, OnClickMinus, i))
						deviceContext->FillRoundedRectangle(areaMinusButton, settings.GetBrushHover(mouse.isDown));
				}
				
				{
					const D2D1_ROUNDED_RECT areaPlusButton = MakeRoundedRect(
						D2D_RECT_F {
							.left = textBoxArea.right,
							.top = textBoxArea.top,
							.right = textBoxArea.right + btnWidth,
							.bottom = textBoxArea.bottom},
						RADIUS);
				
					deviceContext->DrawRoundedRectangle(areaPlusButton, settings.GetBrushUiText(isSelected));
					if (mouse.Hittest(areaPlusButton.rect, this, OnClickPlus, i))
						deviceContext->FillRoundedRectangle(areaPlusButton, settings.GetBrushHover(mouse.isDown));
				}
			}
			
		// draw checkbox
		} else if (item.parameter->type == Parameter::Type_Bool) {
			
			const D2D_RECT_F checkboxArea {
				.left = itemArea.left + (RectWidth(area) / 2.0f) + PADDING,
				.top = itemArea.top + PADDING_X2,
				.right = itemArea.left + (RectWidth(area) / 2.0f) + PADDING + settings.fontUi.lineHeight,
				.bottom = itemArea.bottom - PADDING_X2};
			
			deviceContext->DrawRectangle(checkboxArea, settings.GetBrushUiText(isSelected));
			
			if (item.isChecked) {
				deviceContext->FillRectangle(
					D2D_RECT_F {
						checkboxArea.left + 2u,		
						checkboxArea.top + 2u,
						checkboxArea.right - 2u,		
						checkboxArea.bottom - 3u},
					settings.GetBrushUiText(isSelected));
			}
		
		// draw dropdown
		} else if (item.parameter->type == Parameter::Type_Enum) {
			if (item.parameter->enumValues.empty()) continue;
			
			ASSERT(item.selectedEnumIndex < item.parameter->enumValues.size());
			const std::string& currentValue = item.parameter->enumValues[item.selectedEnumIndex].name;
			
			staticGlyphRun.Shape(currentValue, settings.fontUi);
			
			const f32 x = itemArea.left + (RectWidth(area) / 2.0f);
			const f32 y = itemArea.top;
					
			deviceContext->FillRectangle(
				D2D_RECT_F {
					.left = x + PADDING,
					.top  = y + PADDING,
					.right = itemArea.right - PADDING,
					.bottom = itemArea.bottom - PADDING},
				settings.GetBrushUiBackground(isSelected));
				
			staticGlyphRun.Draw(deviceContext, x + PADDING_X2, y + PADDING_X2, settings.fontUi, settings.GetBrushUiText());
			
			if (isSelected && isDropDownOpen) {
				const D2D_RECT_F dropdownArea {
					.left = x + PADDING,
					.top = itemArea.bottom + PADDING,
					.right = itemArea.right - PADDING,
					.bottom = itemArea.bottom + PADDING_X2 + (settings.fontUi.lineHeight * item.parameter->enumValues.size())};
				BlurArea(deviceContext, dropdownArea);
				
				GlyphRun runValue {};
				for (u64 j = 0u; j < item.parameter->enumValues.size(); j++) {
					const D2D_RECT_F dropDownItemArea {
						.left = dropdownArea.left,
						.top = dropdownArea.top + (settings.fontUi.lineHeight * j) + PADDING,
						.right = dropdownArea.right,
						.bottom = dropdownArea.top + (settings.fontUi.lineHeight * (j+1)) + PADDING};
								
					if (j == item.selectedEnumIndex)
						deviceContext->FillRectangle(dropDownItemArea, settings.GetBrushSelection());
					
					staticGlyphRun.Shape(item.parameter->enumValues[j].name, settings.fontUi);
					staticGlyphRun.Draw(deviceContext, dropDownItemArea.left + PADDING, dropDownItemArea.top, settings.fontUi, settings.GetBrushUiText());
				}
			}
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool CheckRestrictions(const ParameterConfigurator* self) {
	for (u64 i = 0u; i < self->itemCount; i++) {
		ParameterConfigurator::Item& item = self->items[i];
		
		if (item.parameter->type == Parameter::Type_String) {
			if (!item.parameter->allowEmpty
			  && item.textBox.GetText().empty()) return false;
		
		} else if (item.parameter->type == Parameter::Type_Number) {
			int value = 0;
			const std::string_view text = item.textBox.GetText();
			const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
			if (result.ec != std::errc()) return false;
			if (value < item.parameter->minValue) return false;
			if (value > item.parameter->maxValue) return false;
		}
	}
	
	return true;
}

void ParameterConfigurator::GetParameterValues(/*out*/ std::vector<ParameterValue>* values) const {
	ASSERT(CheckRestrictions(this));
	
	values->reserve(itemCount);
	for (u64 i = 0u; i < itemCount; i++) {
		Item& item = items[i];
	
		switch (item.parameter->type) {
			case Parameter::Type_None:
				values->emplace_back(); break;
			case Parameter::Type_String:
				values->push_back(ParameterValue {
					.stringValue = std::string(item.textBox.GetText())}); break;
			case Parameter::Type_Enum:
				values->push_back(ParameterValue {
					.enumIndex = item.selectedEnumIndex}); break;
			case Parameter::Type_Number: {
				int value = 0;
				const std::string_view text = item.textBox.GetText();
				std::from_chars(text.data(), text.data() + text.size(), value);
				values->push_back(ParameterValue {
					.numberValue = value});
			} break;
			case Parameter::Type_Bool:
				values->push_back(ParameterValue {
					.boolValue = item.isChecked}); break;
			default: ASSERT_UNREACHABLE;
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ParameterConfigurator::OnResize() {
	const f32 offsetFromTop = (PADDING_X2 + settings.fontUi.lineHeight) + MARGIN;
	const f32 itemHeight = GetItemHeight();
	
	area = D2D_RECT_F {
		.left = mainWindow.width * 0.4f,
		.top = offsetFromTop,
		.right = mainWindow.width * 0.6f,
		.bottom = offsetFromTop + (itemCount * itemHeight) + (PADDING_X2 + settings.fontUi.lineHeight) };
	
	for (u64 i = 0u; i < itemCount; i++) {
		Item& item = items[i];
		
		const f32 textBoxY = area.top + (itemHeight * i) + PADDING;
		if (item.parameter->type == Parameter::Type_String) {
			item.textBox.width = (RectWidth(area) / 2.0f) - PADDING_X2;
			item.textBox.position = D2D_POINT_2F {
				.x = area.right - PADDING - item.textBox.width,
				.y = textBoxY};
		}
		
		if (item.parameter->type == Parameter::Type_Number) {
			item.textBox.width = (RectWidth(area) / 2.0f) - PADDING_X2 - (2 * (PADDING_X2 + settings.fontUi.lineHeight));
			item.textBox.position = D2D_POINT_2F {
				.x = area.right - PADDING - item.textBox.width - (PADDING_X2 + settings.fontUi.lineHeight),
				.y = textBoxY};
		}
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ParameterConfigurator::OnKeyDown(KeyEvent event, Command command) {
	
	ASSERT(selectedItem <= itemCount);
	Item* item = selectedItem == itemCount ? nullptr : &items[selectedItem];	
	
	if (event.vkeycode == VK_TAB && (event.modifiers & KM_Ctrl) != 0 && (event.modifiers & KM_Alt) != 0) {
		selectedItem = (event.modifiers & KM_Shift) != 0
			? DecrementWrapAround(selectedItem, itemCount+1)
			: IncrementWrapAround(selectedItem, itemCount+1);
		isDropDownOpen = true;
	
	} else if ((event.vkeycode == VK_UP || event.vkeycode == VK_DOWN) && event.modifiers == KM_None) {
		
		// step through enum values
		if (item && item->parameter->type == Parameter::Type_Enum && isDropDownOpen) {
			ASSERT(!item->parameter->enumValues.empty())
			
			item->selectedEnumIndex = event.vkeycode == VK_UP
				? DecrementWrapAround(item->selectedEnumIndex, item->parameter->enumValues.size())
				: IncrementWrapAround(item->selectedEnumIndex, item->parameter->enumValues.size());
		
		// step through parameters
		} else {
			selectedItem = event.vkeycode == VK_UP
				? DecrementWrapAround(selectedItem, itemCount+1)
				: IncrementWrapAround(selectedItem, itemCount+1);
			isDropDownOpen = false;	
		}
	
	} else if ((event.vkeycode == VK_LEFT || event.vkeycode == VK_RIGHT) && event.modifiers == KM_None) {
		
		// increment or decrement numbers
		if (item && item->parameter->type == Parameter::Type_Number && !item->textBox.invalid) {
			
			s64 number = 0;
			const std::string_view text = item->textBox.GetText();
			const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.length(), number);
			
			if (result.ec != std::errc()) return;
			
			number = (event.vkeycode == VK_LEFT)
				? std::max(number - 1, item->parameter->minValue)
			    : std::min(number + 1, item->parameter->maxValue);
			
			item->textBox.SetText(std::to_string(number));
		
		// switch between run and cancel
		} else if (!item) {
			isCancelButtonSelected = (event.vkeycode == VK_LEFT);
		}
	
	} else if (event.vkeycode == VK_RETURN) {
		if (isCancelButtonSelected)     result = Result_Cancel;
		else if (CheckParameters(this)) result = Result_Run;
		else                            result = Result_Unfinished;
	
	// pass through to textbox
 	} else if (item && item->HasTextBox()) {
		bool changed = item->textBox.OnKeyDown(event, command);
		if (changed) {
			const std::string_view text = item->textBox.GetText();		
			
			if (item->parameter->type == Parameter::Type_Number) {
				
				s64 number = 0;
				const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.length(), number);
				
				item->textBox.invalid = text.empty() ||
					(result.ec != std::errc()) ||
					number < item->parameter->minValue ||
					number > item->parameter->maxValue;
			
			} else if (item->parameter->type == Parameter::Type_String) {
				item->textBox.invalid = item->parameter->allowEmpty || !text.empty();
			}
		}
	}
}

void ParameterConfigurator::OnChar(const char* data, u64 len) {
	ASSERT(selectedItem <= itemCount);
	
	if (selectedItem == itemCount) return;
	Item& item = items[selectedItem];	
	
	const bool isSpace = *data == ' ' && len == 1u;
	if (item.parameter->type == Parameter::Type_Bool && isSpace) {
		item.isChecked = !item.isChecked;
	
	} else if (item.parameter->type == Parameter::Type_Enum && isSpace) {
		isDropDownOpen = !isDropDownOpen;
	
	} else if (item.parameter->type == Parameter::Type_Number) {
			
		if (item.textBox.OnChar(data, len)) {
			s64 number = 0;
			const std::string_view text = item.textBox.GetText();
			const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.length(), number);
			
			item.textBox.invalid = (result.ec != std::errc()) && !text.empty();
		}
	
	} else if (item.parameter->type == Parameter::Type_String) {
		item.textBox.OnChar(data, len);
	}
}








