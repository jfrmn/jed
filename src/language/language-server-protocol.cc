#include "language-server-protocol.hh"
#include "json-helper.hh"

#include <cJSON/cJSON.h>

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

#define COMPLETION_ITEM_KIND_MAPPING(X)\
	X(Lsp::CompletionItemKind_Unspecified, "unspecified")\
	X(Lsp::CompletionItemKind_Text, "text")\
	X(Lsp::CompletionItemKind_Method, "method")\
	X(Lsp::CompletionItemKind_Function, "function")\
	X(Lsp::CompletionItemKind_Constructor, "constructor")\
	X(Lsp::CompletionItemKind_Field, "field")\
	X(Lsp::CompletionItemKind_Variable, "variable")\
	X(Lsp::CompletionItemKind_Class, "class")\
	X(Lsp::CompletionItemKind_Interface, "interface")\
	X(Lsp::CompletionItemKind_Module, "module")\
	X(Lsp::CompletionItemKind_Property, "property")\
	X(Lsp::CompletionItemKind_Unit, "unit")\
	X(Lsp::CompletionItemKind_Value, "value")\
	X(Lsp::CompletionItemKind_Enum, "enum")\
	X(Lsp::CompletionItemKind_Keyword, "keyword")\
	X(Lsp::CompletionItemKind_Snippet, "snippet")\
	X(Lsp::CompletionItemKind_Color, "color")\
	X(Lsp::CompletionItemKind_File, "file")\
	X(Lsp::CompletionItemKind_Reference, "reference")\
	X(Lsp::CompletionItemKind_Folder, "folder")\
	X(Lsp::CompletionItemKind_EnumMember, "enumMember")\
	X(Lsp::CompletionItemKind_Constant, "constant")\
	X(Lsp::CompletionItemKind_Struct, "struct")\
	X(Lsp::CompletionItemKind_Event, "event")\
	X(Lsp::CompletionItemKind_Operator, "operator")\
	X(Lsp::CompletionItemKind_TypeParameter, "typeParameter")

#define ENUM_TO_STRING(enumValue, strValue)\
	if (value == enumValue) {\
		*str = strValue;\
		return true;\
	}

#define ENUM_FROM_STRING(enumValue, strValue)\
	if (str && strcmp(str, strValue) == 0) {\
		*value = enumValue;\
		return true;\
	}

//=============================================================================
// GERNERAL STRUCTURES
//=============================================================================

bool ReadJson(const cJSON* json, Lsp::ErrorResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadInteger("code", &value->code);
	reader.ReadString("message", &value->message);
	value->data = reader.Get("data");
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::Position* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadUnsigned("line", &value->line);
	reader.ReadUnsigned("character", &value->character);
	return reader.ok;
}

void WriteJson(const Lsp::Position* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("line")->WriteUnsigned(value->line);
	writeBuffer->WriteProperty("character")->WriteUnsigned(value->character);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::Range* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("start", &value->start);
	reader.ReadValue("end", &value->end);
	return reader.ok;
}

void WriteJson(const Lsp::Range* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("start")->WriteValue(&value->start);
	writeBuffer->WriteProperty("end")->WriteValue(&value->end);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::Location* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	reader.ReadValue("range", &value->range);
	return reader.ok;
}

void WriteJson(const Lsp::Location* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteProperty("range")->WriteValue(&value->range);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::PositionEncodingKind* value) {
	if (!JsonExpectType(json, cJSON_String)) return false;
	POSITION_ENCODING_MAPPING(READ_ENUM_STRING);
	return false;
}

void WriteJson(const Lsp::PositionEncodingKind* value, JsonWriteBuffer* writeBuffer) {
	POSITION_ENCODING_MAPPING(WRITE_ENUM_STRING);
	writeBuffer->WriteNull();
}

bool ReadJson(const cJSON* json, Lsp::TextDocumentIdentifier* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	return reader.ok;
}

void WriteJson(const Lsp::TextDocumentIdentifier* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("uri")->WriteString(value->uri);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::VersionedTextDocumentIdentifier* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	if (const cJSON* jsonVersion = reader.Get("version"))
		value->version = static_cast<s32>(jsonVersion->valuedouble);
	return reader.ok;
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

bool ReadJson(const cJSON* json, Lsp::MarkupKind* value) {
	if (!JsonExpectType(json, cJSON_String)) return false;
	MARKUP_KIND_MAPPING(READ_ENUM_STRING);
	return false;
}

bool ReadJson(const cJSON* json, Lsp::MarkupString* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_String)) return false;
	if (cJSON_IsString(json)) {
		value->kind = Lsp::MarkupKind_Plaintext;
		value->value = json->valuestring ? json->valuestring : "";
		return true;
	
	} else if (cJSON_IsObject(json)) {
		JsonObjectReader reader {json};
		reader.ReadValue("kind", &value->kind);
		reader.ReadString("language", &value->language);
		reader.ReadString("value", &value->value);
		return reader.ok;
	
	} else {
		ASSERT_UNREACHABLE;
		return false;
	}
}

void WriteJson(const Lsp::MarkupString* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("kind")->WriteValue(&value->kind);
	writeBuffer->WriteProperty("value")->WriteString(value->value);
	if (value->kind == Lsp::MarkupKind_Markdown)
		writeBuffer->WriteProperty("language")->WriteString(value->language);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::TextEdit* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("range", &value->range);
	reader.ReadString("newText", &value->newText);
	return reader.ok;
}

void WriteJson(const Lsp::TextEdit* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("range")->WriteValue(&value->range);
	writeBuffer->WriteProperty("newText")->WriteString(value->newText);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::CompletionItemKind* value) {
	if (!JsonExpectType(json, cJSON_String)) return false;
	COMPLETION_ITEM_KIND_MAPPING(READ_ENUM_STRING);
	return false;
}

void WriteJson(const Lsp::CompletionItemKind* value, JsonWriteBuffer* writeBuffer) {
	COMPLETION_ITEM_KIND_MAPPING(WRITE_ENUM_STRING);
}

bool ReadJson(const cJSON* json, Lsp::TraceValue* value) {
	if (!JsonExpectType(json, cJSON_String)) return false;
	TRACE_VALUE_MAPPING(READ_ENUM_STRING);
	return false;
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

bool ReadJson(const cJSON* json, Lsp::GotoServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadBoolean("workDoneProgress", &value->workDoneProgress);
	return reader.ok;
}

template<class TGotoRequest>
static void WriteGotoRequest(const TGotoRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteValue(&value->position);
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

static bool ReadJson(const cJSON* json, Lsp::LocationLink* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	
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
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::GotoResponse* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_Array | cJSON_NULL)) return false;
	
	if (cJSON_IsObject(json)) {
		Lsp::LocationLink& link = value->locations.emplace_back();
		return ReadJson(json, &link);
	
	} else if (cJSON_IsArray(json)) {
		bool ok = true;
		for (cJSON* elem = json->child; elem; elem = elem->next)
			ok &= ReadJson(elem, &value->locations.emplace_back());
		return ok;

	} else if (cJSON_IsNull(json)) {
		return true;
	
	} else {
		ASSERT_UNREACHABLE;
		return false;
	}
}

void WriteJson(const Lsp::GotoReferencesClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::GotoReferencesRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteValue(&value->position);
	writeBuffer->WriteProperty("context")
		->WriteObjectStart()
		->WriteProperty("includeDeclaration")->WriteBoolean(value->context.includeDecleration)
		->WriteObjectEnd();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::CompletionClientCapabilities::CompletionItem* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("snippetSupport")->WriteBoolean(value->snippetSupport);
	writeBuffer->WriteProperty("commitCharactersSupport")->WriteBoolean(value->commitCharactersSupport);
	writeBuffer->WriteProperty("documentationFormat")->WriteArrayStart();
	for (const Lsp::MarkupKind& kind : value->documentationFormat)
		writeBuffer->WriteValue(&kind);
	writeBuffer->WriteArrayEnd();
	writeBuffer->WriteProperty("deprecatedSupport")->WriteBoolean(value->deprecatedSupport);
	writeBuffer->WriteProperty("preselectSupport")->WriteBoolean(value->preselectSupport);
	writeBuffer->WriteProperty("insertReplaceSupport")->WriteBoolean(value->insertReplaceSupport);
	writeBuffer->WriteProperty("labelDetailsSupport")->WriteBoolean(value->labelDetailsSupport);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::CompletionClientCapabilities::ItemKinds* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("valueSet")->WriteArrayStart();
	for (const int item : value->valueSet)
		writeBuffer->WriteInteger(item);
	writeBuffer->WriteArrayEnd();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::CompletionClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("completionItem")->WriteValue(&value->completionItem);
	writeBuffer->WriteProperty("completionItemKind")->WriteValue(&value->completionItemKind);
	writeBuffer->WriteProperty("contextSupport")->WriteBoolean(value->contextSupport);
	writeBuffer->WriteProperty("insertTextMode")->WriteInteger(value->insertTextMode);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::CompletionServerCapabilities::CompletionItem* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadBoolean("labelDetailsSupport", &value->labelDetailsSupport);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::CompletionServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	for (const cJSON* elem = reader.GetArray("triggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
	
	for (const cJSON* elem = reader.GetArray("allCommitCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->allCommitCharacters.push_back(elem->valuestring);
	
	reader.ReadBoolean("resolveProvider", &value->resolveProvider);
	reader.ReadValue("completionItem", &value->completionItem);
	return reader.ok;
}

void WriteJson(const Lsp::CompletionRequest::Context* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("triggerKind")->WriteInteger(value->triggerKind);
	writeBuffer->WriteProperty("triggerCharacter")->WriteString(value->triggerCharacter);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::CompletionRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteValue(&value->position);
	writeBuffer->WriteProperty("context")->WriteValue(&value->context);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::CompletionResponse::Item* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("label", &value->label);
	reader.ReadInteger("kind", &value->kind);
	reader.ReadString("detail", &value->detail);
	reader.ReadValue("documentation", &value->documentation);
	reader.ReadBoolean("deprecated", &value->deprecated);
	reader.ReadBoolean("preselect", &value->preselect);
	reader.ReadString("insertText", &value->insertText);
	reader.ReadString("filterText", &value->filterText);
	reader.ReadValue("textEdit", &value->textEdit);
	reader.ReadFloat("score", &value->clangdScore);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::CompletionResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadBoolean("isIncomplete", &value->isIncomplete);
	
	for (const cJSON* elem = reader.GetArray("items"); elem; elem = elem->next)
		reader.ok &= ReadJson(elem, &value->items.emplace_back());
	
	return reader.ok;
}

void WriteJson(const Lsp::HoverClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("contentFormat")->WriteArrayStart();
	for (const auto& format : value->contentFormat)
		writeBuffer->WriteValue(&format);
	writeBuffer->WriteArrayEnd();
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::HoverServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadBoolean("workDoneProgress", &value->workDoneProgress);
	return reader.ok;
}

void WriteJson(const Lsp::HoverRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteValue(&value->position);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::HoverResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("contents", &value->contents);
	reader.ReadValue("range", &value->range);
	return reader.ok;
}

void WriteJson(const Lsp::DocumentSymbolClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("symbolKind")->WriteObjectStart()->WriteProperty("valueSet")->WriteArrayStart();
	for (const int symbol : value->symbolKind.valueSet)
		writeBuffer->WriteInteger(symbol);
	writeBuffer->WriteArrayEnd()->WriteObjectEnd();
	writeBuffer->WriteProperty("hierarchicalDocumentSymbolSupport")->WriteBoolean(value->hierarchicalDocumentSymbolSupport);
	writeBuffer->WriteProperty("tagSupport")->WriteBoolean(value->tagSupport);
	writeBuffer->WriteProperty("labelSupport")->WriteBoolean(value->labelSupport);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::DocumentSymbolServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("workDoneProgress", &value->label);
	return reader.ok;
}

void WriteJson(const Lsp::DocumentSymbolRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::SymbolInformation* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("name", &value->name);
	reader.ReadInteger("kind", &value->kind);
	reader.ReadValue("location", &value->location);
	reader.ReadBoolean("deprecated", &value->deprecated);
	reader.ReadString("containerName", &value->containerName);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::DocumentSymbolResponse* value) {
	if (!JsonExpectType(json, cJSON_Array)) return false;
	bool ok = true;
	for (const cJSON* elem = json->child; elem; elem = elem->next)
		ok &= ReadJson(elem, &value->symbols.emplace_back());
	return ok;
}

void WriteJson(const Lsp::SignatureHelpClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("signatureInformation")->WriteObjectStart();
	writeBuffer->WriteProperty("parameterInformation")->WriteObjectStart();
	writeBuffer->WriteProperty("labelOffsetSupport")->WriteBoolean(value->signatureInformation.parameterInformation.labelOffsetSupport);
	writeBuffer->WriteObjectEnd();
	writeBuffer->WriteProperty("documentationFormat")->WriteArrayStart();
	for (const Lsp::MarkupKind& kind : value->signatureInformation.documentationFormat)
		writeBuffer->WriteValue(&kind);
	writeBuffer->WriteArrayEnd();
	writeBuffer->WriteProperty("activeParameterSupport")->WriteBoolean(value->signatureInformation.activeParameterSupport);
	writeBuffer->WriteProperty("contextSupport")->WriteBoolean(value->contextSupport);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::SignatureHelpServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	for (const cJSON* elem = reader.GetArray("triggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
	for (const cJSON* elem = reader.GetArray("retriggerCharacters"); elem; elem = elem->next)
		if (elem->valuestring) value->triggerCharacters.push_back(elem->valuestring);
	return reader.ok;
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
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("position")->WriteValue(&value->position);
	writeBuffer->WriteProperty("context")->WriteValue(&value->context);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::SignatureHelpResponse::Signature::Parameter::Label* value) {
	if (!JsonExpectType(json, cJSON_String | cJSON_Array)) return false;
	if (cJSON_IsString(json)) {
		value->isSubstring = false;
		value->string = json->valuestring ? json->valuestring : "";
		return true;
	
	} else if (cJSON_IsArray(json)) {
		const cJSON* first = json->child;
		const cJSON* second = first ? first->next : nullptr;
		if (!first || !second) return false;
		if (!JsonExpectType(first, cJSON_Number) || !JsonExpectType(second, cJSON_Number)) return false;
		value->isSubstring = true;
		value->substring.first = static_cast<int>(first->valuedouble);
		value->substring.second = static_cast<int>(second->valuedouble);
		return true;
		
	} else {
		ASSERT_UNREACHABLE;
		return false;
	}
}

static bool ReadJson(const cJSON* json, Lsp::SignatureHelpResponse::Signature::Parameter* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("label", &value->label);
	reader.ReadValue("documentation", &value->documentation);
	return reader.ok;
}

static bool ReadJson(const cJSON* json, Lsp::SignatureHelpResponse::Signature* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("label", &value->label);
	reader.ReadValue("documentation", &value->documentation);
	for (const cJSON* elem = reader.GetArray("parameters"); elem; elem = elem->next)
		reader.ok &= ReadJson(elem, &value->parameters.emplace_back());
	reader.ReadInteger("activeParameter", &value->activeParameter);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::SignatureHelpResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	for (const cJSON* elem = reader.GetArray("signatures"); elem; elem = elem->next)
		reader.ok &= ReadJson(elem, &value->signatures.emplace_back());
	reader.ReadInteger("activeSignature", &value->activeSignature);
	reader.ReadInteger("activeParameter", &value->activeParameter);
	return reader.ok;
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

bool ReadJson(const cJSON* json, Lsp::TextDocumentSyncServerCapabilities::SaveOptions* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadBoolean("includeText", &value->includeText);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::TextDocumentSyncServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object | cJSON_Object)) return false;
	if (cJSON_IsNumber(json)) {
		value->change = static_cast<int>(json->valuedouble);
		return true;
	
	} else if (cJSON_IsObject(json)) {
		JsonObjectReader reader {json};
		reader.ReadBoolean("openClose", &value->openClose);
		reader.ReadInteger("change", &value->change);
		reader.ReadBoolean("willSave", &value->willSave);
		reader.ReadValue("save", &value->save.emplace());
		return reader.ok;
	
	} else {
		ASSERT_UNREACHABLE;
		return false;
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
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidCloseTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidChangeTextDocumentNotification::ChangeEvent* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	if (value->range.has_value())
		writeBuffer->WriteProperty("range")->WriteValue(&value->range.value());
	writeBuffer->WriteProperty("text")->WriteString(value->text);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidChangeTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("contentChanges")->WriteArrayStart();
	for (const Lsp::DidChangeTextDocumentNotification::ChangeEvent& change : value->contentChanges)
		writeBuffer->WriteValue(&change);
	writeBuffer->WriteArrayEnd();
	if (value->clangdWantDiagnostics.has_value())
		writeBuffer->WriteProperty("wantDiagnostics")->WriteBoolean(value->clangdWantDiagnostics.value());
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::WillSaveTextDocumentNotification::SaveReason* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteInteger(*value);
}

void WriteJson(const Lsp::WillSaveTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("reason")->WriteValue(&value->saveReason);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::DidSaveTextDocumentNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	if (value->text.has_value())
		writeBuffer->WriteProperty("text")->WriteString(value->text.value());
	writeBuffer->WriteObjectEnd();
}

//=============================================================================
// WINDOW FEATURES
//=============================================================================

void WriteJson(const Lsp::ShowMessageRequestClientCapabilities::MessageActionItem* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("additionalPropertiesSupport")->WriteBoolean(value->additionalPropertiesSupport);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::ShowMessageRequestClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("messageActionItem")->WriteValue(&value->messageActionItem);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::MessageNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadInteger("type", reinterpret_cast<int*>(&value->type));
	reader.ReadString("message", &value->message);
	return reader.ok;
}

void WriteJson(const Lsp::PublishDiagnosticsClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("relatedInformation")->WriteBoolean(value->relatedInforamtion);
	writeBuffer->WriteProperty("versionSupport")->WriteBoolean(value->versionSupport);
	writeBuffer->WriteProperty("codeDescriptionSupport")->WriteBoolean(value->codeDescriptionSupport);
	writeBuffer->WriteProperty("dataSupport")->WriteBoolean(value->dataSupport);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::Diagnostic* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("range", &value->range);
	reader.ReadInteger("severity", &value->severity);
	reader.ReadString("code", &value->code);
	reader.ReadString("message", &value->message);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::PublishDiagnosticsNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("uri", &value->uri);
	reader.ReadInteger("version", &value->version);
	for (const cJSON* elem = reader.GetArray("diagnostics"); elem; elem = elem->next)
		reader.ok &= ReadJson(elem, &value->diagnostics.emplace_back());
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::LogTraceNotification* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("message", &value->message);
	reader.ReadString("verbose", &value->verbose);
	return reader.ok;
}

void WriteJson(const Lsp::SetTraceParams* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("value")->WriteValue(&value->value);
	writeBuffer->WriteObjectEnd();
}

//=============================================================================
// LIFECYCLE
//=============================================================================

void WriteJson(const Lsp::InitializeRequest::ClientInfo* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("name")->WriteString(value->name);
	if (!value->version.empty())
		writeBuffer->WriteProperty("version")->WriteString(value->version);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::InitializeRequest::ClientCapabilities::TextDocumentClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("synchronization")->WriteValue(&value->synchronization);
	writeBuffer->WriteProperty("completion")->WriteValue(&value->completion);
	writeBuffer->WriteProperty("hover")->WriteValue(&value->hover);
	writeBuffer->WriteProperty("declaration")->WriteValue(&value->declaration);
	writeBuffer->WriteProperty("definition")->WriteValue(&value->definition);
	writeBuffer->WriteProperty("typeDefinition")->WriteValue(&value->typeDefinition);
	writeBuffer->WriteProperty("implementation")->WriteValue(&value->implementation);
	writeBuffer->WriteProperty("references")->WriteValue(&value->references);
	writeBuffer->WriteProperty("publishDiagnostics")->WriteValue(&value->publishDiagnostics);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::InitializeRequest::ClientCapabilities::Window* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("workDoneProgress")->WriteBoolean(value->workDoneProgress);
	writeBuffer->WriteProperty("showMessage")->WriteValue(&value->showMessage);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::InitializeRequest::ClientCapabilities::General* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("positionEncodings")->WriteArrayStart();
	for (const Lsp::PositionEncodingKind& encoding : value->positionEncodings)
		writeBuffer->WriteValue(&encoding);
	writeBuffer->WriteArrayEnd();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::InitializeRequest::ClientCapabilities* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("textDocument")->WriteValue(&value->textDocument);
	writeBuffer->WriteProperty("window")->WriteValue(&value->window);
	writeBuffer->WriteProperty("general")->WriteValue(&value->general);
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::InitializeRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteProperty("processId")->WriteInteger(value->processId);
	writeBuffer->WriteProperty("clientInfo")->WriteValue(&value->clientInfo);
	writeBuffer->WriteProperty("locale")->WriteString(value->locale);
	writeBuffer->WriteProperty("rootPath")->WriteString(value->rootPath);
	writeBuffer->WriteProperty("capabilities")->WriteValue(&value->capabilities);
	if (value->clangdOffsetEncoding.has_value()) {
		writeBuffer->WriteProperty("offsetEncoding")->WriteArrayStart();
		for (const Lsp::PositionEncodingKind& encoding : value->clangdOffsetEncoding.value())
			writeBuffer->WriteValue(&encoding);
		writeBuffer->WriteArrayEnd();
	}
	writeBuffer->WriteProperty("trace")->WriteValue(&value->trace);
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::InitializeResponse::ServerCapabilities* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("positionEncoding", &value->positionEncoding);
	reader.ReadValue("textDocumentSync", &value->textDocumentSync.emplace());
	reader.ReadValue("completionProvider", &value->completionProvider.emplace());
	reader.ReadValue("hoverProvider", &value->hoverProvider.emplace());
	reader.ReadValue("signatureHelpProvider", &value->signatureHelpProvider.emplace());
	reader.ReadValue("declarationProvider", &value->declarationProvider.emplace());
	reader.ReadValue("definitionProvider", &value->definitionProvider.emplace());
	reader.ReadValue("typeDefinitionProvider", &value->typeDefinitionProvider.emplace());
	reader.ReadValue("implementationProvider", &value->implementationProvider.emplace());
	reader.ReadValue("referencesProvider", &value->referencesProvider.emplace());
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::InitializeResponse::ServerInfo* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadString("name", &value->name);
	reader.ReadString("version", &value->version);
	return reader.ok;
}

bool ReadJson(const cJSON* json, Lsp::InitializeResponse* value) {
	if (!JsonExpectType(json, cJSON_Object)) return false;
	JsonObjectReader reader {json};
	reader.ReadValue("capabilities", &value->capabilities);
	reader.ReadValue("serverInfo", &value->serverInfo);
	return reader.ok;
}

void WriteJson(const Lsp::ShutdownRequest* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

bool ReadJson(const cJSON* json, Lsp::ShutdownResponse* value) {
	return JsonExpectType(json, cJSON_NULL | cJSON_Object);
}

void WriteJson(const Lsp::InitializedNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}

void WriteJson(const Lsp::ExitNotification* value, JsonWriteBuffer* writeBuffer) {
	writeBuffer->WriteObjectStart();
	writeBuffer->WriteObjectEnd();
}
