#include "editor-signaturehelp.hh"
#include "editor/editor.hh"
#include "globals.hh"
#include "events.hh"

#include "ui/style.hh"
#include "ui/constants.h"

#include "util/rect-util.hh"
#include "util/format.hh"
#include "graphics/effects.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1_1.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorSignatureHelp* EditorSignatureHelp::Make(Editor* owner) {
	auto self = new EditorSignatureHelp();
	self->owner = owner;
	self->triggerPosition = owner->textController.carets.front().position;
	return self;
}

void EditorSignatureHelp::OnUpdate() {
	if (DrawIncompleteState(deviceContext, "No signatures.")) return;
		
	ASSERT(activeSignature >= 0u && activeSignature < signatureCount);
	Signature* signature = &signatures[activeSignature];

	GlyphRun runLabel {};
	GlyphRunMultiline runDocumentation {};
	
	runLabel.Shape(signature->label, style.fontEditor, &owner->glyphRunShapingMemory);
	runDocumentation.Shape(signature->documentation, style.fontUi, &owner->glyphRunShapingMemory);
		
	const Parameter* parameter = nullptr;
	
	//
	// get active parameter
	//
	{
		const u64 parameterIndex = signature->activeParameter != U64_MAX
			? signature->activeParameter
			: this->activeSignature;
			
		if (parameterIndex < signature->parameter.size())
			parameter = &signature->parameter[parameterIndex];
	}
	
	//
	// calc size
	//	
	const f32 width = PADDING_X2 + std::max(
		runLabel.GetTotalAdvance(),
		runDocumentation.GetMaxAdvance());
	
	const f32 height = PADDING_X2 + style.fontEditor.lineHeight +
		(runDocumentation.GetLineCount() > 0u || signatureCount > 1u
			? style.fontUi.lineHeight * std::max<u64>(1u, runDocumentation.GetLineCount())
			: 0.0f);
	
	//
	// position
	//
	const D2D1_POINT_2F caretPosition = owner->GetCaretLocation();
	const D2D_POINT_2F position {
		.x = caretPosition.x,
		.y = autocompleteIsActive
			? caretPosition.y - TOOLTIP_CURSOR_EXTRA_OFFSET_Y - height
			: caretPosition.y + style.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y};
	
	//
	// draw background
	//
	{
		BlurArea(deviceContext, MakeRect(position.x, position.y, width, height));
	}
		
	//
	// draw signature
	//
	{
		runLabel.Draw(deviceContext, {position.x + PADDING, position.y + PADDING}, style.fontEditor, style.GetBrushUiText());
		
		if (parameter && parameter->labelIsSubstring) {
			
			f32 offsetFrom, offsetTo;
			runLabel.GetGlyphOffsetRange(parameter->labelOffset, parameter->labelOffset + parameter->labelLength, &offsetFrom, &offsetTo);
		
			deviceContext->DrawLine(
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetFrom,
					.y = position.y + style.fontEditor.underlineOffset},
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetTo,
					.y = position.y + style.fontEditor.underlineOffset},
				style.GetBrushUiText());
		}
				
		runDocumentation.Draw(deviceContext, {position.x + PADDING, position.y + PADDING + style.fontEditor.lineHeight}, style.fontUi, style.GetBrushUiText(false));
		
		if (signatureCount > 1u) {
			const std::string text = FormatString("%/%", activeSignature + 1u, signatureCount);
			
			GlyphRun runSignatureIndex {};
			runSignatureIndex.Shape(text, style.fontUi, &owner->glyphRunShapingMemory);
			runSignatureIndex.Draw(deviceContext, {position.x + width - PADDING - runSignatureIndex.GetTotalAdvance(), position.y + PADDING + style.fontEditor.lineHeight}, style.fontUi, style.GetBrushUiText());
		}
	}
}

bool EditorSignatureHelp::OnKeyDown(KeyEvent event) {
	if (state != State_Completed) return false;
	
	if (signatureCount > 1u && (event.vkeycode == VK_DOWN || event.vkeycode == VK_UP) && event.alt) {
		activeSignature = event.vkeycode == VK_DOWN
			? IncrementWrapAround(activeSignature, signatureCount)
			: DecrementWrapAround(activeSignature, signatureCount);
		return true;
	}
	
	return false;
}

void EditorSignatureHelp::OnInput() {
	
	const TextPosition cursor = owner->textController.carets.front().position;
	if (cursor.line != triggerPosition.line) {
		
		ASSERT(!autocompleteIsActive)
		ASSERT(owner->editorCaretAttached == this)
		owner->editorCaretAttached = nullptr;
		
		RemoveReference();
	}
}

EditorSignatureHelp::~EditorSignatureHelp() noexcept {
	for (u64 i = 0u; i < parameterCount; i++) {
		if (!parameters[i].labelIsSubstring) 
			delete[] parameters[i].labelData;
	}
	
	delete[] parameters;
	delete[] signatures;
}