#include "language-server-protocol.hh"
#include "json-helper.hh"

#include <cJSON.h>

#define MARKUP_KIND_MAPPING(X)\
	X(Lsp::MarkupKind_Plaintext, "plaintext")\
	X(Lsp::MarkupKind_Markdown, "markdown")

#define POSITION_ENCODING_MAPPING(X)\
	X(Lsp::PositionEncodingKind_Utf8, "utf-8")\
	X(Lsp::PositionEncodingKind_Utf16, "utf-16")\
	X(Lsp::PositionEncodingKind_Utf32, "utf-32")

#define TRACE_VALUE_MAPPING(X)\
	X(Lsp::TraceValue_Off, "off")\
	X(Lsp::TraceValue_Messages, "messages")\
	X(Lsp::TraceValue_Verbose, "verbose")

#define WRITE_ENUM_STRING(enumValue, strValue)\
	if (*value == enumValue) {\
		writeBuffer->WriteEnumString(strValue);\
		return;\
	}

#define READ_ENUM_STRING(enumValue, strValue)\
	if (json->valuestring && strcmp(json->valuestring, strValue) == 0) {\
		*value = enumValue;\
		return;\
	}
	
template<class T>
static void ReadJson(const cJSON* json, std::optional<T>* optionalValue) {
	if (cJSON_IsNull(json)) return;
	
	if constexpr (!std::is_same_v<T, bool>) {
		if (cJSON_IsFalse(json)) return;
		if (cJSON_IsTrue(json)) {
			optionalValue->emplace();
			return;
		}
	}
	
	return ReadJson(json, &optionalValue->emplace());
}

//=============================================================================
// GERNERAL STRUCTURES
//=============================================================================

void ReadJson(const cJSON* json, Lsp::ErrorResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadInteger("code", &value->code);
	reader.ReadString("message", &value->message);
	value->data = reader.Get("data");
}

void ReadJson(const cJSON* json, Lsp::Position* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadUnsigned("line", &value->line);
	reader.ReadUnsigned("character", &value->character);
}

void WriteJson(const Lsp::Position* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("line")->WriteUnsigned(value->line);
	writeBuffer->WriteProperty("character")->WriteUnsigned(value->character);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::Range* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("start", &value->start);
	reader.ReadValue("end", &value->end);
}

void WriteJson(const Lsp::Range* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("start")->WriteCustom(&value->start);
	writeBuffer->WriteProperty("end")->WriteCustom(&value->end);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::Location* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	reader.ReadValue("range", &value->range);
}

void WriteJson(const Lsp::Location* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteProperty("range")->WriteCustom(&value->range);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::PositionEncodingKind* value) {
	if (!JsonExpectType(json, cJSON_String)) return;
	POSITION_ENCODING_MAPPING(READ_ENUM_STRING);
}

void WriteJson(const Lsp::PositionEncodingKind* value, JsonWriteBuffer* writeBuffer) {
	POSITION_ENCODING_MAPPING(WRITE_ENUM_STRING);
	writeBuffer->WriteNull();
}

void ReadJson(const cJSON* json, Lsp::TextDocumentIdentifier* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
}

void WriteJson(const Lsp::TextDocumentIdentifier* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::VersionedTextDocumentIdentifier* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	if (const cJSON* jsonVersion = reader.Get("version"))
		value->version = static_cast<s32>(jsonVersion->valuedouble);
}

void WriteJson(const Lsp::VersionedTextDocumentIdentifier* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteProperty("version")->WriteInteger(value->version);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::MarkupKind* value, JsonWriteBuffer* writeBuffer) {
	MARKUP_KIND_MAPPING(WRITE_ENUM_STRING);
}

void ReadJson(const cJSON* json, Lsp::MarkupKind* value) {
	if (!JsonExpectType(json, cJSON_String)) return;
	MARKUP_KIND_MAPPING(READ_ENUM_STRING);
}

void ReadJson(const cJSON* json, Lsp::MarkupString* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_String)) return;
	if (cJSON_IsString(json)) {
		value->kind = Lsp::MarkupKind_Plaintext;
		value->value = json->valuestring ? json->valuestring : "";
	
	} else if (cJSON_IsObject(json)) {
		JsonObjectReader reader {json};
		reader.ReadValue("kind", &value->kind);
		reader.ReadString("language", &value->language);
		reader.ReadString("value", &value->value);
	
	} else {
		ASSERT_UNREACHABLE;
	}
}

void WriteJson(const Lsp::MarkupString* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("kind")->WriteCustom(&value->kind);
	writeBuffer->WriteProperty("value")->WriteString(value->value);
	if (value->kind == Lsp::MarkupKind_Markdown)
		writeBuffer->WriteProperty("language")->WriteString(value->language);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::TextEdit* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("range", &value->range);
	reader.ReadString("newText", &value->newText);
}

void WriteJson(const Lsp::TextEdit* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("range")->WriteCustom(&value->range);
	writeBuffer->WriteProperty("newText")->WriteString(value->newText);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::TraceValue* value) {
	if (!JsonExpectType(json, cJSON_String)) return;
	TRACE_VALUE_MAPPING(READ_ENUM_STRING);
}

void WriteJson(const Lsp::TraceValue* value, JsonWriteBuffer* writeBuffer) {
	TRACE_VALUE_MAPPING(WRITE_ENUM_STRING);
}

//=============================================================================
// LANGUAGE FEATURES
//=============================================================================

void WriteJson(const Lsp::GotoClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("linkSupport")->WriteBoolean(value->linkSupport);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::GotoServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadBoolean("workDoneProgress", &value->workDoneProgress);
}

template<class TGotoRequest>
static void WriteGotoRequest(const TGotoRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteCustom(&value->position);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::GotoDeclerationRequest* value, JsonWriteBuffer* writeBuffer) {
	WriteGotoRequest(value, writeBuffer);
}

void WriteJson(const Lsp::GotoDefinitionRequest* value, JsonWriteBuffer* writeBuffer) {
	WriteGotoRequest(value, writeBuffer);
}

void WriteJson(const Lsp::GotoTypeDefinitionRequest* value, JsonWriteBuffer* writeBuffer) {
	WriteGotoRequest(value, writeBuffer);
}

void WriteJson(const Lsp::GotoImplementationRequest* value, JsonWriteBuffer* writeBuffer) {
	WriteGotoRequest(value, writeBuffer);
}

static void ReadJson(const cJSON* json, Lsp::LocationLink* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	
	JsonObjectReader reader {json};
	if (!reader.Contains("originSelectionRange") && !reader.Contains("targetRange") && !reader.Contains("targetUri")) {
		value->isSimpleLocation = true;
		reader.ReadString("uri", &value->targetUri);
		reader.ReadValue("range", &value->targetSelectionRange);
	
	} else {
		value->isSimpleLocation = false;
		reader.ReadValue("originSelectionRange", &value->originSelectionRange);
		reader.ReadString("targetUri", &value->targetUri);
		reader.ReadValue("targetRange", &value->targetRange);
		reader.ReadValue("targetSelectionRange", &value->targetSelectionRange);
	}
}

void ReadJson(const cJSON* json, Lsp::GotoResponse* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_Array | cJSON_NULL)) return;
	
	if (cJSON_IsObject(json)) {
		Lsp::LocationLink& link = value->locations.emplace_back();
		return ReadJson(json, &link);
	
	} else if (cJSON_IsArray(json)) {
		for (cJSON* elem = json->child; elem; elem = elem->next)
			ReadJson(elem, &value->locations.emplace_back());

	} else if (cJSON_IsNull(json)) {
		return;
	
	} else {
		ASSERT_UNREACHABLE;
	}
}

void WriteJson(const Lsp::GotoReferencesClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::GotoReferencesRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteCustom(&value->position);
	writeBuffer->WriteProperty("context");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("includeDeclaration")->WriteBoolean(value->context.includeDecleration);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::CompletionClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("completionItem");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("snippetSupport")->WriteBoolean(value->completionItem.snippetSupport);
		writeBuffer->WriteProperty("commitCharactersSupport")->WriteBoolean(value->completionItem.commitCharactersSupport);
		writeBuffer->WriteProperty("documentationFormat");
		{
			writeBuffer->WriteArrayStart();
			for (const Lsp::MarkupKind& kind : value->completionItem.documentationFormat)
				writeBuffer->WriteCustom(&kind);
			writeBuffer->WriteArrayEnd();
		}
		writeBuffer->WriteProperty("deprecatedSupport")->WriteBoolean(value->completionItem.deprecatedSupport);
		writeBuffer->WriteProperty("preselectSupport")->WriteBoolean(value->completionItem.preselectSupport);
		writeBuffer->WriteProperty("insertReplaceSupport")->WriteBoolean(value->completionItem.insertReplaceSupport);
		writeBuffer->WriteProperty("labelDetailsSupport")->WriteBoolean(value->completionItem.labelDetailsSupport);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteProperty("completionItemKind");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("valueSet");
		{
			writeBuffer->WriteArrayStart();
			for (const int item : value->completionItemKind.valueSet)
				writeBuffer->WriteInteger(item);
			writeBuffer->WriteArrayEnd();
		}
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteProperty("contextSupport")->WriteBoolean(value->contextSupport);
	writeBuffer->WriteProperty("insertTextMode")->WriteInteger(value->insertTextMode);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::CompletionServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	
	JsonObjectReader reader {json};
	for (const cJSON* elem = reader.GetArray("triggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
	
	for (const cJSON* elem = reader.GetArray("allCommitCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->allCommitCharacters.push_back(elem->valuestring);
	
	reader.ReadBoolean("resolveProvider", &value->resolveProvider);
	
	if (const cJSON* elem = reader.GetObject("completionItem")) {
		JsonObjectReader reader {json};
		reader.ReadBoolean("labelDetailsSupport", &value->completionItem.labelDetailsSupport);
	}
	
}

void WriteJson(const Lsp::CompletionRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteCustom(&value->position);
	writeBuffer->WriteProperty("context");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("triggerKind")->WriteInteger(value->context.triggerKind);
		writeBuffer->WriteProperty("triggerCharacter")->WriteString(value->context.triggerCharacter);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::CompletionResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadBoolean("isIncomplete", &value->isIncomplete);
	
	for (const cJSON* elem = reader.GetArray("items"); elem; elem = elem->next) {
		if (!JsonExpectType(elem, cJSON_Object)) continue;		
		JsonObjectReader reader {elem};	
		Lsp::CompletionResponse::Item& item = value->items.emplace_back();
		reader.ReadString("label", &item.label);
		reader.ReadInteger("kind", &item.kind);
		reader.ReadString("detail", &item.detail);
		reader.ReadValue("documentation", &item.documentation);
		reader.ReadBoolean("deprecated", &item.deprecated);
		reader.ReadBoolean("preselect", &item.preselect);
		reader.ReadString("insertText", &item.insertText);
		reader.ReadString("filterText", &item.filterText);
		reader.ReadValue("textEdit", &item.textEdit);
		reader.ReadFloat("score", &item.clangdScore);
	}
}

void WriteJson(const Lsp::HoverClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("contentFormat");
	{
		writeBuffer->WriteArrayStart();
		for (const auto& format : value->contentFormat)
			writeBuffer->WriteCustom(&format);
		writeBuffer->WriteArrayEnd();
	}
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::HoverServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadBoolean("workDoneProgress", &value->workDoneProgress);
}

void WriteJson(const Lsp::HoverRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteCustom(&value->position);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::HoverResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("contents", &value->contents);
	reader.ReadValue("range", &value->range);
}

void WriteJson(const Lsp::DocumentSymbolClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("symbolKind");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("valueSet");
		{
			writeBuffer->WriteArrayStart();
			for (const int symbol : value->symbolKind.valueSet)
				writeBuffer->WriteInteger(symbol);
			writeBuffer->WriteArrayEnd();
		}
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteProperty("hierarchicalDocumentSymbolSupport")->WriteBoolean(value->hierarchicalDocumentSymbolSupport);
	writeBuffer->WriteProperty("tagSupport")->WriteBoolean(value->tagSupport);
	writeBuffer->WriteProperty("labelSupport")->WriteBoolean(value->labelSupport);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::DocumentSymbolServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("label", &value->label);
}

void WriteJson(const Lsp::DocumentSymbolRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::SymbolInformation* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("name", &value->name);
	reader.ReadInteger("kind", &value->kind);
	reader.ReadValue("location", &value->location);
	reader.ReadBoolean("deprecated", &value->deprecated);
	reader.ReadString("containerName", &value->containerName);
}

void ReadJson(const cJSON* json, Lsp::DocumentSymbolResponse* value) {
	if (!JsonExpectType(json, cJSON_Array)) return;
	for (const cJSON* elem = json->child; elem; elem = elem->next)
		ReadJson(elem, &value->symbols.emplace_back());
}

void WriteJson(const Lsp::SignatureHelpClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("signatureInformation");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("parameterInformation");
		{
			writeBuffer->WriteObjectStart();
			writeBuffer->WriteProperty("labelOffsetSupport")->WriteBoolean(value->signatureInformation.parameterInformation.labelOffsetSupport);
			writeBuffer->WriteObjectEnd();
		}
		writeBuffer->WriteProperty("documentationFormat");
		{
			writeBuffer->WriteArrayStart();
			for (const Lsp::MarkupKind& kind : value->signatureInformation.documentationFormat)
				writeBuffer->WriteCustom(&kind);
			writeBuffer->WriteArrayEnd();
		}
		writeBuffer->WriteProperty("activeParameterSupport")->WriteBoolean(value->signatureInformation.activeParameterSupport);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteProperty("contextSupport")->WriteBoolean(value->contextSupport);
}

void ReadJson(const cJSON* json, Lsp::SignatureHelpServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	for (const cJSON* elem = reader.GetArray("triggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
	for (const cJSON* elem = reader.GetArray("retriggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
}

void WriteJson(const Lsp::SignatureHelpRequest::Context* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	if (value->triggerKind != Lsp::SignatureHelpRequest::Context::TriggerKind_Unknown)
		writeBuffer->WriteProperty("triggerKind")->WriteInteger(value->triggerKind);
	if (!value->triggerCharacter.empty())
		writeBuffer->WriteProperty("triggerCharacter")->WriteString(value->triggerCharacter);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::SignatureHelpRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteCustom(&value->position);
	writeBuffer->WriteProperty("context")->WriteCustom(&value->context);
	writeBuffer->WriteObjectEnd();
}

static void ReadJson(const cJSON* json, Lsp::SignatureHelpResponse::Signature::Parameter::Label* value) {
	if (!JsonExpectType(json, cJSON_String | cJSON_Array)) return;
	if (cJSON_IsString(json)) {
		value->isSubstring = false;
		value->string = json->valuestring ? json->valuestring : "";
	
	} else if (cJSON_IsArray(json)) {
		const cJSON* first = json->child;
		const cJSON* second = first ? first->next : nullptr;
		if (!first || !second) return;
		if (!JsonExpectType(first, cJSON_Number) || !JsonExpectType(second, cJSON_Number)) return;
		value->isSubstring = true;
		value->substring.first = static_cast<int>(first->valuedouble);
		value->substring.second = static_cast<int>(second->valuedouble);
		
	} else {
		ASSERT_UNREACHABLE;
	}
}

void ReadJson(const cJSON* json, Lsp::SignatureHelpResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	for (const cJSON* sig = reader.GetArray("signatures"); sig; sig = sig->next) {
		if (!JsonExpectType(json, cJSON_Object)) continue;
		JsonObjectReader reader {sig};
		Lsp::SignatureHelpResponse::Signature& signature = value->signatures.emplace_back();
		
		reader.ReadString("label", &signature.label);
		reader.ReadValue("documentation", &signature.documentation);
		for (const cJSON* param = reader.GetArray("parameters"); param; param = param->next) {
			JsonObjectReader reader {param};
			Lsp::SignatureHelpResponse::Signature::Parameter& parameter = signature.parameters.emplace_back();
			reader.ReadValue("label", &parameter.label);
			reader.ReadValue("documentation", &parameter.documentation);
		}
		reader.ReadInteger("activeParameter", &value->activeParameter);
	}
	reader.ReadInteger("activeSignature", &value->activeSignature);
	reader.ReadInteger("activeParameter", &value->activeParameter);
}

//=============================================================================
// DOCUMENT SYNCHRONIZATION
//=============================================================================

void WriteJson(const Lsp::TextDocumentSyncClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("willSave")->WriteBoolean(value->willSave);
	writeBuffer->WriteProperty("willSaveWaitUntil")->WriteBoolean(value->willSaveWaitUntil);
	writeBuffer->WriteProperty("didSave")->WriteBoolean(value->didSave);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::TextDocumentSyncServerCapabilities::SaveOptions* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadBoolean("includeText", &value->includeText);
}

void ReadJson(const cJSON* json, Lsp::TextDocumentSyncServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_Object)) return;
	if (cJSON_IsNumber(json)) {
		value->change = static_cast<int>(json->valuedouble);
	
	} else if (cJSON_IsObject(json)) {
		JsonObjectReader reader {json};
		reader.ReadBoolean("openClose", &value->openClose);
		reader.ReadInteger("change", &value->change);
		reader.ReadBoolean("willSave", &value->willSave);
		reader.ReadValue("save", &value->save);
	
	} else {
		ASSERT_UNREACHABLE;
	}
}

void WriteJson(const Lsp::TextDocumentItem* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteProperty("languageId")->WriteString(value->languageId);
	writeBuffer->WriteProperty("version")->WriteInteger(value->version);
	writeBuffer->WriteProperty("text")->WriteString(value->text);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidOpenTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidCloseTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidChangeTextDocumentNotification::ChangeEvent* value, JsonWriteBuffer* writeBuffer) {
	
}

void WriteJson(const Lsp::DidChangeTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("contentChanges")->WriteArrayStart();
	for (const Lsp::DidChangeTextDocumentNotification::ChangeEvent& change : value->contentChanges)
	{
	
		writeBuffer->WriteObjectStart();
		if (change.range.has_value())
			writeBuffer->WriteProperty("range")->WriteCustom(&change.range.value());
		writeBuffer->WriteProperty("text")->WriteString(change.text);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteArrayEnd();
	
	if (value->clangdWantDiagnostics.has_value())
		writeBuffer->WriteProperty("wantDiagnostics")->WriteBoolean(value->clangdWantDiagnostics.value());
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::WillSaveTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	writeBuffer->WriteProperty("reason")->WriteInteger(value->saveReason);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidSaveTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteCustom(&value->textDocument);
	if (value->text.has_value())
		writeBuffer->WriteProperty("text")->WriteString(value->text.value());
	writeBuffer->WriteObjectEnd();
}

//=============================================================================
// WINDOW FEATURES
//=============================================================================

void WriteJson(const Lsp::ShowMessageRequestClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("messageActionItem");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("additionalPropertiesSupport")->WriteBoolean(value->messageActionItem.additionalPropertiesSupport);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::MessageNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadInteger("type", reinterpret_cast<int*>(&value->type));
	reader.ReadString("message", &value->message);
}

void WriteJson(const Lsp::PublishDiagnosticsClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("relatedInformation")->WriteBoolean(value->relatedInforamtion);
	writeBuffer->WriteProperty("versionSupport")->WriteBoolean(value->versionSupport);
	writeBuffer->WriteProperty("codeDescriptionSupport")->WriteBoolean(value->codeDescriptionSupport);
	writeBuffer->WriteProperty("dataSupport")->WriteBoolean(value->dataSupport);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::Diagnostic* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("range", &value->range);
	reader.ReadInteger("severity", &value->severity);
	reader.ReadString("code", &value->code);
	reader.ReadString("message", &value->message);
}

void ReadJson(const cJSON* json, Lsp::PublishDiagnosticsNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	reader.ReadInteger("version", &value->version);
	for (const cJSON* elem = reader.GetArray("diagnostics"); elem; elem = elem->next)
		ReadJson(elem, &value->diagnostics.emplace_back());
}

void ReadJson(const cJSON* json, Lsp::LogTraceNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("message", &value->message);
	reader.ReadString("verbose", &value->verbose);
}

void WriteJson(const Lsp::SetTraceParams* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("value")->WriteCustom(&value->value);
	writeBuffer->WriteObjectEnd();
}

//=============================================================================
// LIFECYCLE
//=============================================================================

void WriteJson(const Lsp::InitializeRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("processId")->WriteInteger(value->processId);
	writeBuffer->WriteProperty("clientInfo");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("name")->WriteString(value->clientInfo.name);
		if (!value->clientInfo.version.empty())
			writeBuffer->WriteProperty("version")->WriteString(value->clientInfo.version);
		writeBuffer->WriteObjectEnd();
	}
	writeBuffer->WriteProperty("locale")->WriteString(value->locale);
	writeBuffer->WriteProperty("rootPath")->WriteString(value->rootPath);
	writeBuffer->WriteProperty("capabilities");
	{
		writeBuffer->WriteObjectStart();
		writeBuffer->WriteProperty("textDocument");
		{
			writeBuffer->WriteObjectStart();
			writeBuffer->WriteProperty("synchronization")->WriteCustom(&value->capabilities.textDocument.synchronization);
			writeBuffer->WriteProperty("completion")->WriteCustom(&value->capabilities.textDocument.completion);
			writeBuffer->WriteProperty("hover")->WriteCustom(&value->capabilities.textDocument.hover);
			writeBuffer->WriteProperty("declaration")->WriteCustom(&value->capabilities.textDocument.declaration);
			writeBuffer->WriteProperty("definition")->WriteCustom(&value->capabilities.textDocument.definition);
			writeBuffer->WriteProperty("typeDefinition")->WriteCustom(&value->capabilities.textDocument.typeDefinition);
			writeBuffer->WriteProperty("implementation")->WriteCustom(&value->capabilities.textDocument.implementation);
			writeBuffer->WriteProperty("references")->WriteCustom(&value->capabilities.textDocument.references);
			writeBuffer->WriteProperty("publishDiagnostics")->WriteCustom(&value->capabilities.textDocument.publishDiagnostics);
			writeBuffer->WriteObjectEnd();
		}
		writeBuffer->WriteProperty("window");
		{
			writeBuffer->WriteObjectStart();
			writeBuffer->WriteProperty("workDoneProgress")->WriteBoolean(value->capabilities.window.workDoneProgress);
			writeBuffer->WriteProperty("showMessage")->WriteCustom(&value->capabilities.window.showMessage);
			writeBuffer->WriteObjectEnd();
		}
		writeBuffer->WriteProperty("general");
		{
			writeBuffer->WriteObjectStart();
			writeBuffer->WriteProperty("positionEncodings")->WriteArrayStart();
			for (Lsp::PositionEncodingKind encoding : value->capabilities.general.positionEncodings)
				writeBuffer->WriteCustom(&encoding);
			writeBuffer->WriteArrayEnd();
			writeBuffer->WriteObjectEnd();
		}
		writeBuffer->WriteObjectEnd();
	}
	
	if (value->clangdOffsetEncoding.has_value()) {
		writeBuffer->WriteProperty("offsetEncoding")->WriteArrayStart();
		for (Lsp::PositionEncodingKind encoding : value->clangdOffsetEncoding.value())
			writeBuffer->WriteCustom(&encoding);
		writeBuffer->WriteArrayEnd();
	}
	writeBuffer->WriteProperty("trace")->WriteCustom(&value->trace);
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::InitializeResponse::ServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("positionEncoding", &value->positionEncoding);
	reader.ReadValue("textDocumentSync", &value->textDocumentSync);
	reader.ReadValue("completionProvider", &value->completionProvider);
	reader.ReadValue("hoverProvider", &value->hoverProvider);
	reader.ReadValue("signatureHelpProvider", &value->signatureHelpProvider);
	reader.ReadValue("declarationProvider", &value->declarationProvider);
	reader.ReadValue("definitionProvider", &value->definitionProvider);
	reader.ReadValue("typeDefinitionProvider", &value->typeDefinitionProvider);
	reader.ReadValue("implementationProvider", &value->implementationProvider);
	reader.ReadValue("referencesProvider", &value->referencesProvider);
}

void ReadJson(const cJSON* json, Lsp::InitializeResponse::ServerInfo* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadString("name", &value->name);
	reader.ReadString("version", &value->version);
}

void ReadJson(const cJSON* json, Lsp::InitializeResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return;
	JsonObjectReader reader {json};
	reader.ReadValue("capabilities", &value->capabilities);
	reader.ReadValue("serverInfo", &value->serverInfo);
}

void WriteJson(const Lsp::ShutdownRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

void ReadJson(const cJSON* json, Lsp::ShutdownResponse* value) {
	JsonExpectType(json, cJSON_NULL | cJSON_Object);
}

void WriteJson(const Lsp::InitializedNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::ExitNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

const char* Str(Lsp::ErrorResponse::Code code) {
	switch (code) {
		case Lsp::ErrorResponse::Code_Success: return "Success";
		case Lsp::ErrorResponse::Code_ClientParseError: return "ClientParseError";
		case Lsp::ErrorResponse::Code_ClientInconclusiveMessage: return "ClientInconclusiveMessage";
		case Lsp::ErrorResponse::Code_ParseError: return "ParseError";
		case Lsp::ErrorResponse::Code_InvalidRequest: return "InvalidRequest";
		case Lsp::ErrorResponse::Code_MethodNotFound: return "MethodNotFound";
		case Lsp::ErrorResponse::Code_InvalidParams: return "InvalidParams";
		case Lsp::ErrorResponse::Code_InternalError: return "InternalError";
		case Lsp::ErrorResponse::Code_ServerNotInitialized: return "ServerNotInitialized";
		case Lsp::ErrorResponse::Code_UnknownErrorCode: return "UnknownErrorCode";
		case Lsp::ErrorResponse::Code_RequestFailed: return "RequestFailed";
		case Lsp::ErrorResponse::Code_ServerCancelled: return "ServerCancelled";
		case Lsp::ErrorResponse::Code_ContentModified: return "ContentModified";
		case Lsp::ErrorResponse::Code_RequestCancelled: return "RequestCancelled";
		default: return "??";
	}
}
