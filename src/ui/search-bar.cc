#include "search-bar.hh"
#include "basic.hh"
#include "main-window.hh"
#include "globals.hh"
#include "events.hh"
#include "theme.hh"

#include "ui/constants.h"
#include "util/rect-util.hh"
#include "graphics/effects.hh"
#include "commands/parameter-configurator.hh"

#include <cmath>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr float ITEM_HIGHLIGHT_OPACITY_VALUE_MAX = (F32_PI * 2.0f) * 10.0f; // 10 cylces
static constexpr float ITEM_HIGHLIGHT_OPACITY_SPEED = 0.004f;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static float GetItemHeight() {
	return MARGIN_X2 + (theme.fontUi.lineHeight * 2);
}

static void UpdateItems(SearchBar* self) {
	const std::string_view searchText = self->textBox.GetText();
	
	const u64 itemCountBefore = self->itemCount;
	self->itemCount = self->FilterItems(searchText);;

	self->scrollarea.totalSize.height = self->itemCount * GetItemHeight();	
	if (itemCountBefore != self->itemCount)
		self->OnResize();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool SearchBar::Init(std::string_view placeholderText, bool updateItemsImmediately) {
	
	if (!textBox.Init(&theme.fontUi, placeholderText))
		return false;
	
	scrollarea.vpX = 0.0f;	
	scrollarea.vpY = 0.0f;
	scrollarea.barWidth = SCROLLBAR_WIDTH_NARROW;
	
	shouldClose = false;
	OnResize();
	
	if (updateItemsImmediately)
		UpdateItems(this);
	
	return true;
}

SearchBar::~SearchBar() noexcept {
	delete parameterConfigurator;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnClickItem(void* ud, u64 i) {
	auto self = static_cast<SearchBar*>(ud);
	self->AcceptItem(i, nullptr);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnUpdate() {

	if (parameterConfigurator) {
		parameterConfigurator->OnUpdate();
		return;
	}
		
	//
	// update animation
	//
	{
		itemHighlightAnimationValue += ITEM_HIGHLIGHT_OPACITY_SPEED * deltaTime;
		if (itemHighlightAnimationValue > ITEM_HIGHLIGHT_OPACITY_VALUE_MAX)
			itemHighlightAnimationValue = ITEM_HIGHLIGHT_OPACITY_VALUE_MAX;
		else needsUpdate = true;
	}
		
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
	}
		
	//
	// textbox
	//
	{
		textBox.OnUpdate();
	}
	
	PopLayer(deviceContext);
	
	//
	// draw items
	//
	{
		const f32 itemHeight = GetItemHeight();
		const f32 itemAreaTop = area.top + MARGIN_X2 + textBox.Height();
		
		deviceContext->PushAxisAlignedClip(
			D2D_RECT_F {
				.left = area.left,
				.top = itemAreaTop,
				.right = area.right,
				.bottom = area.bottom},
			D2D1_ANTIALIAS_MODE_ALIASED);
		DEFER(deviceContext->PopAxisAlignedClip());
			
		for (u64 i = 0u; i < itemCount; i++) {
			ItemInfo item;
			GetItemInfo(i, &item);
			
			const D2D_RECT_F itemArea {
				.left   = area.left,
				.top    = itemAreaTop - scrollarea.vpY + (itemHeight * i),
				.right  = area.right,
				.bottom = itemAreaTop - scrollarea.vpY + (itemHeight * (i+1))};
				
			if (selectedItem == i) {
				ID2D1SolidColorBrush* brushGlow = theme.GetBrushGlow();
				const f32 opacityBefore = brushGlow->GetOpacity();
				DEFER(brushGlow->SetOpacity(opacityBefore));
				
				const f32 opacity = std::sin(itemHighlightAnimationValue) * 0.4f + 0.5f;
				brushGlow->SetOpacity(opacity);
				
				deviceContext->FillRectangle(itemArea, brushGlow);
			}
			
			staticGlyphRun.Shape(item.text, theme.fontUi);
			
			f32 offsetFrom, offsetTo;
			staticGlyphRun.MeasureOffsetRange(
				item.matchedPosition,
			    item.matchedPosition + item.matchedLength,
			    &offsetFrom, &offsetTo);
			
			deviceContext->FillRectangle(
				D2D1_RECT_F {
					.left   = MARGIN + itemArea.left + offsetFrom,
				    .top    = MARGIN + itemArea.top,
				    .right  = MARGIN + itemArea.left + offsetTo,
				    .bottom = MARGIN + itemArea.top + theme.fontUi.lineHeight},
				theme.GetBrushUiSearchResult());

			staticGlyphRun.Draw(deviceContext,
				MARGIN + itemArea.left,
			    MARGIN + itemArea.top,
				theme.fontUi,
				theme.GetBrushUiText());
				
			staticGlyphRun.ShapeAndDraw(deviceContext, 
				item.subText,
				MARGIN + itemArea.left,
			    MARGIN + itemArea.top + theme.fontUi.lineHeight,
				theme.fontUi,
				theme.GetBrushUiText(false));
			
			if (mouse.Hittest(itemArea, this, OnClickItem)) {
				deviceContext->FillRectangle(itemArea, theme.GetBrushHover(mouse.isDown));
			}
		}
	}
	
	//
	// draw scrollbar
	//
	scrollarea.OnUpdate();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnResize() {
 	const f32 offsetFromTop = (PADDING_X2 + theme.fontUi.lineHeight) + MARGIN;
 	const f32 itemAreaTop = offsetFromTop + MARGIN_X2 + textBox.Height();
	 	
 	area = D2D_RECT_F {
		.left   = mainWindow.width * 0.3f,
		.top    = offsetFromTop,
		.right  = mainWindow.width * 0.7f,
		.bottom = std::min(
			itemAreaTop + (itemCount * GetItemHeight()),
			mainWindow.height - MARGIN)};
	
	textBox.position = D2D_POINT_2F {
		area.left + MARGIN,
		area.top + MARGIN},
	textBox.width = RectWidth(area) - MARGIN_X2;
	
	scrollarea.OnResize(D2D_RECT_F {
		.left = area.left,
		.top = itemAreaTop,
		.right = area.right,
		.bottom = mainWindow.height - MARGIN});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnKeyDown(KeyEvent event) {
	if (parameterConfigurator) {
		parameterConfigurator->OnKeyDown(event);
		if (parameterConfigurator->result != ParameterConfigurator::Result_Unfinished)
			OnFinishedParameterConfiguration();
		return;
	}
	
	if ((event.vkeycode == VK_DOWN || event.vkeycode == VK_UP)) {

		if (event.vkeycode == VK_DOWN) {
			selectedItem = IncrementWrapAround(selectedItem, itemCount);

		} else if (event.vkeycode == VK_UP) {
			selectedItem = DecrementWrapAround(selectedItem, itemCount);
		}
		
		itemHighlightAnimationValue = .0f;
	
	} else if (event.vkeycode == VK_RETURN) {
		
		if (selectedItem < itemCount)
			AcceptItem(selectedItem, &event);
		   
	} else if (event.vkeycode == VK_ESCAPE && event.NoModifiers()) {
		shouldClose = true;
	
	} else {
		if (textBox.OnKeyDown(event))
			UpdateItems(this);
	}
}

void SearchBar::OnChar(const char* data, u64 len) {
	if (parameterConfigurator) {
		parameterConfigurator->OnChar(data, len);
	
	} else if (textBox.OnChar(data, len)) {
		UpdateItems(this);
	}
}

void SearchBar::OnMouseWheel(f32 distance) {
	if (parameterConfigurator) return;
	scrollarea.ScrollVertical(distance * theme.fontUi.lineHeight * 5);
}
