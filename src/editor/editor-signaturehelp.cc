#include "editor-signaturehelp.hh"
#include "editor/editor.hh"
#include "globals.hh"
#include "events.hh"
#include "settings.hh"
#include "util.hh"

#include "graphics/effects.hh"
#include "ui/constants.h"

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

void EditorSignatureHelp::Update() {
	if (DrawIncompleteState(deviceContext, "No signatures.")) return;
		
	ASSERT(activeSignature >= 0u && activeSignature < signatureCount);
	Signature* signature = &signatures[activeSignature];

	GlyphRun runLabel {};
	GlyphRunMultiline runDocumentation {};
	
	runLabel.Shape(signature->label, settings.fontEditor);
	runDocumentation.Shape(signature->documentation, settings.fontUi);
		
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
		runLabel.width,
		runDocumentation.GetWidth());
	
	const f32 height = PADDING_X2 + settings.fontEditor.lineHeight +
		(runDocumentation.LineCount() > 0u || signatureCount > 1u
			? settings.fontUi.lineHeight * std::max<u64>(1u, runDocumentation.LineCount())
			: 0.0f);
	
	//
	// position
	//
	const D2D1_POINT_2F caretPosition = owner->GetCaretLocation();
	const D2D_POINT_2F position {
		.x = caretPosition.x,
		.y = autocompleteIsActive
			? caretPosition.y - TOOLTIP_CURSOR_EXTRA_OFFSET_Y - height
			: caretPosition.y + settings.fontEditor.lineHeight + TOOLTIP_CURSOR_EXTRA_OFFSET_Y};
	
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
		runLabel.Draw(deviceContext, position.x + PADDING, position.y + PADDING, settings.fontEditor, settings.GetBrushUiText());
		
		if (parameter && parameter->labelIsSubstring) {
			
			f32 offsetFrom, offsetTo;
			runLabel.MeasureOffsetRange(parameter->labelOffset, parameter->labelOffset + parameter->labelLength, &offsetFrom, &offsetTo);
		
			deviceContext->DrawLine(
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetFrom,
					.y = position.y + settings.fontEditor.underlineOffset},
				D2D1_POINT_2F {
					.x = position.x + PADDING + offsetTo,
					.y = position.y + settings.fontEditor.underlineOffset},
				settings.GetBrushUiText());
		}
				
		runDocumentation.Draw(deviceContext, position.x + PADDING, position.y + PADDING + settings.fontEditor.lineHeight, settings.fontUi, settings.GetBrushUiText(false));
		
		if (signatureCount > 1u) {
			const std::string text = FormatString("%u/%u", activeSignature + 1u, signatureCount);
			
			GlyphRun runSignatureIndex {};
			runSignatureIndex.Shape(text, settings.fontUi);
			runSignatureIndex.Draw(deviceContext, position.x + width - PADDING - runSignatureIndex.width, position.y + PADDING + settings.fontEditor.lineHeight, settings.fontUi, settings.GetBrushUiText());
		}
	}
}

bool EditorSignatureHelp::HandleEvent(const Event& event, const Command& command) {
	if (state != State_Completed) return false;
	
	const bool handleEvent = signatureCount > 1
						  && event.type == Event::Type_KeyPress
						  && (event.vkcode == VK_DOWN || event.vkcode == VK_UP)
						  && event.kmods == KM_Alt;
	
	if (handleEvent) {
		activeSignature = event.vkcode == VK_DOWN
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