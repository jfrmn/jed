#include "editor-gotoline.hh"
#include "editor.hh"
#include "globals.hh"
#include "events.hh"

#include "ui/constants.h"
#include "ui/style.hh"

#include "util/logging.hh"
#include "graphics/effects.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define MAX_DIGITS 10

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorGotoLine* EditorGotoLine::Make(Editor* editor) {
	
	auto self = std::make_unique<EditorGotoLine>();
	self->owner = editor;

	// init textbox
	if (!self->textbox.Init(&style.fontEditor, "Line")) {
		LogError("failed to initalize textbox");
		return nullptr;
	}

	if (!self->glyphRunHeadline.Shape("Goto Line", style.fontUi)) {
		LogError("shaping headline failed");
		return nullptr;
	}
	
	return self.release();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void EditorGotoLine::OnUpdate() {

	//
	// calc size
	//
	const f32 heightHeadline = style.fontUi.lineHeight;
	const f32 totalHeight = MARGIN + heightHeadline + MARGIN + textbox.Height() + MARGIN;
	const f32 textBoxWidth = (style.fontEditor.GetSpaceAdvance() * MAX_DIGITS) + PADDING_X2;
	
	area = D2D_RECT_F {
		.left   = owner->area.right - MARGIN - SCROLLBAR_WIDTH_WIDE - textBoxWidth - MARGIN_X2,
		.top    = owner->area.top   + MARGIN,
		.right  = owner->area.right - MARGIN - SCROLLBAR_WIDTH_WIDE,
		.bottom = owner->area.top   + MARGIN + totalHeight };
	
	textbox.position = D2D_POINT_2F {
		.x = area.left + MARGIN,
		.y = area.top  + MARGIN_X2 + heightHeadline };
	textbox.width = textBoxWidth;

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
	
	DEFER(PopLayer(deviceContext));	
	
	//
	// draw headline
	//
	glyphRunHeadline.Draw(deviceContext,
	    D2D1_POINT_2F {
	    	.x = area.left + MARGIN, 
	    	.y = area.top  + MARGIN },
	    style.fontUi,
	    style.GetBrushUiText());
	
	deviceContext->DrawLine(
		D2D1_POINT_2F {
			.x = area.left + MARGIN,
			.y = area.top  + MARGIN + style.fontUi.lineHeight },
		D2D1_POINT_2F {
			.x = area.left + MARGIN + glyphRunHeadline.GetTotalAdvance(),
			.y = area.top  + MARGIN + style.fontUi.lineHeight },
		style.GetBrushUiText());

	//
	// draw textbox
	//
	textbox.OnUpdate();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static u64 GetCurrentLineNumber(EditorGotoLine* self) {
	
	const std::string_view text = self->textbox.GetText();
	if (text.empty()) return true;

	u64 lineNumber = 0;
	std::from_chars_result res = std::from_chars(text.data(), text.data() + text.size(), lineNumber);

	if (res.ec != std::errc()) {
		LogError("failed to parse line numnber '%'. Error: %", text, FFromCharsResult(res));
		return 0;
	}

	return lineNumber;
}

static void ValidateLineNumber(EditorGotoLine *self) {
	const u64 lineNumber = GetCurrentLineNumber(self);
	self->textbox.invalid = (lineNumber < 1 || lineNumber > self->owner->GetBuffer().LineCount());
}

bool EditorGotoLine::OnKeyDown(KeyEvent event) {
	
	if (event.vkeycode == VK_RETURN) {

		if (textbox.invalid) return true;

		const u64 lineNumber = GetCurrentLineNumber(this) - 1; // adjust to 0-based
		ASSERT(lineNumber > 0 && lineNumber < owner->GetBuffer().LineCount());

		// if we are already at the line then we close ourself
		if (lineNumber == owner->textController.carets.front().position.line) {
			ASSERT(owner->toolWindow == this);
			owner->toolWindow = nullptr;
			delete this;
			return true;
		}

		const TextBuffer::Line& line = owner->GetBuffer().GetLineAt(lineNumber);
		
		usize lineStart = line.GetText().find_first_not_of(" \t\v");
		if (lineStart == std::string_view::npos)
			lineStart = 0u;

		owner->textController.SetCaretPosition(TextPosition {lineNumber, lineStart});
		owner->ScrollToLine(lineNumber);
		return true;
	
	}
	
	if (textbox.OnKeyDown(event)) {
		ValidateLineNumber(this);
		return false;
	}
	
	return false;
}

bool EditorGotoLine::OnChar(const char* data, u64 len) {
		
	if (len == 1 && (data[0] < '0' || data[0] > '9')) return true;
	if (textbox.GetText().size() >= MAX_DIGITS) return true;
 
	if (textbox.OnChar(data, len))
		ValidateLineNumber(this);
	
	return true;
}

bool EditorGotoLine::IsGotoLine() const {
	return true;
}