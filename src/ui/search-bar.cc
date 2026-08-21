#include "search-bar.hh"
#include "basic.hh"
#include "main-window.hh"
#include "globals.hh"
#include "events.hh"
#include "settings.hh"
#include "util.hh"

#include "ui/constants.h"
#include "graphics/effects.hh"
#include "ui/parameter-configurator.hh"

#include <cmath>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr f32 ITEM_HIGHLIGHT_OPACITY_VALUE_MAX = (F32_PI * 2.0f) * 10.0f; // 10 cylces
static constexpr f32 ITEM_HIGHLIGHT_OPACITY_SPEED = 0.004f;

static constexpr f32 SPAWN_ANIMATION_MAX = 1.0f;
static constexpr f32 SPAWN_ANIMATION_SPEED = 0.008f;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static float ItemHeight() {
	return MARGIN_X2 + (settings.fontUi.lineHeight * 2);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool SearchBar::Init(std::string_view placeholderText, bool filterItemsImmediately) {
	
	if (!textBox.Init(&settings.fontUi, placeholderText))
		return false;
	
	scrollarea.vpX = 0.0f;	
	scrollarea.vpY = 0.0f;
	scrollarea.barWidth = SCROLLBAR_WIDTH_NARROW;
	
	shouldClose = false;
	OnResize();
	
	if (filterItemsImmediately)
		FilterItems(std::string_view {});
	
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
		
		spawnAnimationValue += SPAWN_ANIMATION_SPEED * deltaTime;
		if (spawnAnimationValue > SPAWN_ANIMATION_MAX)
			spawnAnimationValue = SPAWN_ANIMATION_MAX;
		else needsUpdate = true;
	}
	
	//
	// spawn animation
	//
	//LogDevVar(spawnAnimationValue);
	const f32 halfWidth = RectWidth(area) / 2.0f;
	const D2D_RECT_F animatedArea {
		.left = area.left + halfWidth - (halfWidth * spawnAnimationValue),
		.top = area.top,
		.right = area.right - halfWidth + (halfWidth * spawnAnimationValue),
		.bottom = area.bottom};
	
	//
	// draw background
	//
	{
		ID2D1Bitmap* background = CopyFromRenderTarget(deviceContext, animatedArea);
		if (!background) return;
		DEFER(background->Release());
		
		DrawGlow(deviceContext, background, animatedArea);
		
		PushLayer(deviceContext, animatedArea);
		BlurArea(deviceContext, animatedArea, background);
	}
		
	//
	// textbox
	//
	{
		textBox.OnUpdate();
	}
	
	//
	// draw items
	//
	{	
		const f32 itemHeight = ItemHeight();
		
		const u64 firstVisible = std::max<u64>(0u, static_cast<u64>(scrollarea.vpY / itemHeight));
		const u64 lastVisible  = std::min<u64>(itemCount, static_cast<u64>((scrollarea.vpY + scrollarea.vpSize.height) / itemHeight) + 1u);
		
		OnUpdateItems(firstVisible, lastVisible);
	}
	
	PopLayer(deviceContext);
	
	//
	// draw scrollbar
	//
	scrollarea.OnUpdate();
}

void SearchBar::UpdateItem(u64 i, const SearchBar::UpdateItemParams& params) {
	const f32 itemHeight = ItemHeight();
	const f32 itemAreaTop = area.top + MARGIN_X2 + textBox.Height();
	
	const D2D_RECT_F itemArea {
		.left   = area.left,
		.top    = itemAreaTop - scrollarea.vpY + (itemHeight * i),
		.right  = area.right,
		.bottom = itemAreaTop - scrollarea.vpY + (itemHeight * (i+1))};
		
	if (selectedItem == i) {
		ID2D1SolidColorBrush* brushGlow = settings.GetBrushDropShadow();
		const f32 opacityBefore = brushGlow->GetOpacity();
		DEFER(brushGlow->SetOpacity(opacityBefore));
		
		const f32 opacity = std::sin(itemHighlightAnimationValue) * 0.4f + 0.5f;
		brushGlow->SetOpacity(opacity);
		
		deviceContext->FillRectangle(itemArea, brushGlow);
	}
	
	staticGlyphRun.Shape(params.text, settings.fontUi);
	
	f32 offsetFrom, offsetTo;
	staticGlyphRun.MeasureOffsetRange(
		params.matchedPosition,
	    params.matchedPosition + params.matchedLength,
	    &offsetFrom, &offsetTo);
	
	deviceContext->FillRectangle(
		D2D1_RECT_F {
			.left   = MARGIN + itemArea.left + offsetFrom,
		    .top    = MARGIN + itemArea.top,
		    .right  = MARGIN + itemArea.left + offsetTo,
		    .bottom = MARGIN + itemArea.top + settings.fontUi.lineHeight},
		settings.GetBrushUiSearchResult());

	staticGlyphRun.Draw(deviceContext,
		MARGIN + itemArea.left,
	    MARGIN + itemArea.top,
		settings.fontUi,
		settings.GetBrushUiText());
		
	staticGlyphRun.ShapeAndDraw(deviceContext, 
		params.subText,
		MARGIN + itemArea.left,
	    MARGIN + itemArea.top + settings.fontUi.lineHeight,
		settings.fontUi,
		settings.GetBrushUiText(false));
	
	if (mouse.Hittest(itemArea, this, OnClickItem))
		deviceContext->FillRectangle(itemArea, settings.GetBrushHover(mouse.isDown));
}

void SearchBar::SetItemCount(u64 newItemCount) {
	if (itemCount != newItemCount) {
		itemCount = newItemCount;
		scrollarea.totalSize.height = itemCount * ItemHeight();
		area.bottom = std::min(
			area.top + MARGIN_X2 + textBox.Height() + (itemCount * ItemHeight()),
			mainWindow.height - MARGIN);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnResize() {
 	const f32 offsetFromTop = (PADDING_X2 + settings.fontUi.lineHeight) + MARGIN;
 	const f32 itemAreaTop = offsetFromTop + MARGIN_X2 + textBox.Height();
	 	
 	area = D2D_RECT_F {
		.left   = mainWindow.width * 0.3f,
		.top    = offsetFromTop,
		.right  = mainWindow.width * 0.7f,
		.bottom = std::min(
			itemAreaTop + (itemCount * ItemHeight()),
			mainWindow.height - MARGIN)};
	
	textBox.position = D2D_POINT_2F {
		.x = area.left + MARGIN,
		.y = area.top + MARGIN},
	textBox.width = RectWidth(area) - MARGIN_X2;
	
	scrollarea.OnResize(D2D_RECT_F {
		.left = area.left,
		.top = itemAreaTop,
		.right = area.right,
		.bottom = mainWindow.height - MARGIN});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SearchBar::OnKeyDown(KeyEvent event, Command command) {
	if (parameterConfigurator) {
		parameterConfigurator->OnKeyDown(event, command);
		if (parameterConfigurator->result != ParameterConfigurator::Result_Unfinished)
			OnFinishedParameterConfiguration();
	}
	
	if ((event.vkeycode == VK_DOWN || event.vkeycode == VK_UP)) {
		if (itemCount == 0u) {
			selectedItem = U64_MAX;
		
		} else if (event.vkeycode == VK_DOWN) {
			selectedItem = IncrementWrapAround(selectedItem, itemCount);

		} else if (event.vkeycode == VK_UP) {
			selectedItem = DecrementWrapAround(selectedItem, itemCount);
		}
		
		itemHighlightAnimationValue = .0f;
	
	} else if (event.vkeycode == VK_RETURN) {
		
		if (selectedItem < itemCount)
			AcceptItem(selectedItem, &event);
		   
	} else if (event.vkeycode == VK_ESCAPE && event.modifiers == KM_None) {
		shouldClose = true;
	
	} else {
		const bool changed = textBox.OnKeyDown(event, command);
		if (changed) FilterItems(textBox.GetText());
	}
}

void SearchBar::OnChar(const char* data, u64 len) {
	if (parameterConfigurator) {
		parameterConfigurator->OnChar(data, len);
	
	} else if (textBox.OnChar(data, len)) {
		FilterItems(textBox.GetText());
	}
}

void SearchBar::OnMouseWheel(f32 distance) {
	if (parameterConfigurator) return;
	scrollarea.ScrollVertical(distance * settings.fontUi.lineHeight * 5);
}
