#include "language-server-protocol.hh"
#include "json/json-mapping.hh"
#include "json/json-mapping-stl.h"

#include <cJSON/cJSON.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<class T>
static bool JsonToValue(const JsonTrace* trace, const cJSON* json, std::optional<T>* result) {
	
	if (!JsonCheckType(trace, json, cJSON_True | cJSON_False | cJSON_Object))
		return false;

	if (cJSON_IsTrue(json)) {
		result->emplace();
		return true;
	
	} else if (cJSON_IsFalse(json)) {
		return true;
	
	} else if (cJSON_IsObject(json)) {
		return JsonToValue(trace, json, &result->emplace());

	} else {
		ASSERT_UNREACHABLE;
		return false;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//=============================================================================
// GERNERAL STRUCTURES
//=============================================================================

// Error Response

JSON_TO_VALUE_BEGIN(Lsp::ErrorResponse)
	JSON_TO_VALUE_PROPERTY(code)
	JSON_TO_VALUE_PROPERTY(message)
JSON_TO_VALUE_END


// Position

#define Position_Properties(X)\
	X(line)\
	X(character)

JSON_TO_VALUE_BEGIN(Lsp::Position)
	Position_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::Position)
	Position_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


// Range

#define Range_Properties(X)\
	X(start)\
	X(end)

JSON_TO_VALUE_BEGIN(Lsp::Range)
	Range_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::Range)
	Range_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


// Location

#define Location_Properties(X)\
	X(uri)\
	X(range)

JSON_TO_VALUE_BEGIN(Lsp::Location)
	Location_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::Location)
	Location_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


// PositionEncoding

#define PositionEncodingKind_Properties(X)\
	X("utf-8", PositionEncodingKind_Utf8)\
	X("utf-16", PositionEncodingKind_Utf16)\
	X("utf-32", PositionEncodingKind_Utf32)

JSON_TO_ENUM_BEGIN(Lsp::PositionEncodingKind)
	PositionEncodingKind_Properties(JSON_TO_ENUM_MEMBER)
JSON_TO_ENUM_END

JSON_FROM_ENUM_BEGIN(Lsp::PositionEncodingKind)
	PositionEncodingKind_Properties(JSON_FROM_ENUM_MEMBER)
JSON_FROM_ENUM_END


// TextDocumentIdentifier

#define TextDocumenIdentifier_Properties(X)\
	X(uri)

JSON_TO_VALUE_BEGIN(Lsp::TextDocumentIdentifier)
	TextDocumenIdentifier_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::TextDocumentIdentifier)
	TextDocumenIdentifier_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


// TextDocumentIdentifier

#define VersionedTextDocumenIdentifier_Properties(X)\
	X(uri)\
	X(version)

JSON_TO_VALUE_BEGIN(Lsp::VersionedTextDocumentIdentifier)
	VersionedTextDocumenIdentifier_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::VersionedTextDocumentIdentifier)
	VersionedTextDocumenIdentifier_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END

// MarkupKind

#define MarkupKind_Properties(X)\
	X("plaintext", MarkupKind_Plaintext)\
	X("markdown", MarkupKind_Markdown)

JSON_TO_ENUM_BEGIN(Lsp::MarkupKind)
	MarkupKind_Properties(JSON_TO_ENUM_MEMBER)
JSON_TO_ENUM_END

JSON_FROM_ENUM_BEGIN(Lsp::MarkupKind)
	MarkupKind_Properties(JSON_FROM_ENUM_MEMBER)
JSON_FROM_ENUM_END


// MarkupString

#define MarkupString_Properties(X)\
	X(kind)\
	X(language)\
	X(value)

bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, Lsp::MarkupString* result) {

	if (cJSON_IsObject(json)) {

		std::unordered_map<std::string_view, cJSON*> properties {};
		if (!JsonObjectToMap(parentTrace, json, &properties))
			return false;

		MarkupString_Properties(JSON_TO_VALUE_PROPERTY)
		return true;
	
	} else if (cJSON_IsString(json)) {
		if (!JsonToValue(parentTrace, json, &result->value)) return false;

		result->kind = Lsp::MarkupKind::MarkupKind_Plaintext;
		return true;
	
	} else {
		JsonLogError(parentTrace, "expected json to be an [object] or a [string] but was [%]", JsonTypeToString(json->type));
		return false;
	}
}

JSON_FROM_VALUE_BEGIN(Lsp::MarkupString)
	MarkupString_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


// TextEdit

#define TextEdit_Properties(X)\
	X(range)\
	X(newText)

JSON_TO_VALUE_BEGIN(Lsp::TextEdit)
	TextEdit_Properties(JSON_TO_VALUE_PROPERTY)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::TextEdit)
	TextEdit_Properties(JSON_FROM_VALUE_PROPERTY)
JSON_FROM_VALUE_END


//=============================================================================
// LANGUAGE FEATURES
//=============================================================================

// Goto

JSON_FROM_VALUE_BEGIN(Lsp::GotoClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(linkSupport)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::GotoReferencesClientCapabilities)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::GotoServerCapabilities)
	JSON_TO_VALUE_PROPERTY(workDoneProgress)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::GotoDeclerationRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::GotoDefinitionRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::GotoTypeDefinitionRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::GotoImplementationRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::LocationLink)
	if (!properties.contains("originSelectionRange") && !properties.contains("targetRange") && !properties.contains("targetUri"))
		result->isSimpleLocation = true;
	
	JSON_TO_VALUE_PROPERTY(originSelectionRange)
	JSON_TO_VALUE_PROPERTY(targetUri)
	JSON_TO_VALUE_PROPERTY(targetRange)
	JSON_TO_VALUE_PROPERTY(targetSelectionRange)
	JSON_TO_VALUE_PROPERTY_NAMED("range", targetSelectionRange)
	JSON_TO_VALUE_PROPERTY_NAMED("uri", targetUri)
JSON_TO_VALUE_END

bool JsonToValue(const JsonTrace* parentTrace, const cJSON *json, Lsp::GotoResponse* result) {
	
	ASSERT(json && result);
	
	if (cJSON_IsObject(json)) {
		Lsp::LocationLink &location = result->locations.emplace_back();
		return JsonToValue(parentTrace, json, &location);
	
	} else if (cJSON_IsArray(json)) {
		return JsonToValue(parentTrace, json, &result->locations);
	
	} else if (cJSON_IsNull(json)) {
		return true;
	
	} else {
		JsonLogError(parentTrace, "expected value to be either an [object], [array] or [null] but was [%]", JsonTypeToString(json->type));
		return false;
	}
}

// Completion

#define CompletionItemKind_Properties(X)\
	X("unspecified", CompletionItemKind_Unspecified)\
	X("text", CompletionItemKind_Text)\
	X("method", CompletionItemKind_Method)\
	X("function", CompletionItemKind_Function)\
	X("constructor", CompletionItemKind_Constructor)\
	X("field", CompletionItemKind_Field)\
	X("variable", CompletionItemKind_Variable)\
	X("class", CompletionItemKind_Class)\
	X("interface", CompletionItemKind_Interface)\
	X("module", CompletionItemKind_Module)\
	X("property", CompletionItemKind_Property)\
	X("unit", CompletionItemKind_Unit)\
	X("value", CompletionItemKind_Value)\
	X("enum", CompletionItemKind_Enum)\
	X("keyword", CompletionItemKind_Keyword)\
	X("snippet", CompletionItemKind_Snippet)\
	X("color", CompletionItemKind_Color)\
	X("file", CompletionItemKind_File)\
	X("reference", CompletionItemKind_Reference)\
	X("folder", CompletionItemKind_Folder)\
	X("enumMember", CompletionItemKind_EnumMember)\
	X("constant", CompletionItemKind_Constant)\
	X("struct", CompletionItemKind_Struct)\
	X("event", CompletionItemKind_Event)\
	X("operator", CompletionItemKind_Operator)\
	X("typeParameter", CompletionItemKind_TypeParameter)

JSON_TO_ENUM_BEGIN(Lsp::CompletionItemKind)
	CompletionItemKind_Properties(JSON_TO_ENUM_MEMBER)
JSON_TO_ENUM_END

JSON_FROM_ENUM_BEGIN(Lsp::CompletionItemKind)
	CompletionItemKind_Properties(JSON_FROM_ENUM_MEMBER)
JSON_FROM_ENUM_END

JSON_FROM_VALUE_BEGIN(Lsp::CompletionClientCapabilities::ItemKinds)
	JSON_FROM_VALUE_PROPERTY(valueSet)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::CompletionClientCapabilities::CompletionItem)
	JSON_FROM_VALUE_PROPERTY(snippetSupport)
	JSON_FROM_VALUE_PROPERTY(commitCharactersSupport)
	JSON_FROM_VALUE_PROPERTY(documentationFormat)
	JSON_FROM_VALUE_PROPERTY(deprecatedSupport)
	JSON_FROM_VALUE_PROPERTY(preselectSupport)
	JSON_FROM_VALUE_PROPERTY(insertReplaceSupport)
	JSON_FROM_VALUE_PROPERTY(labelDetailsSupport)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::CompletionClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(completionItem)
	JSON_FROM_VALUE_PROPERTY(completionItemKind)
	JSON_FROM_VALUE_PROPERTY(contextSupport)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::CompletionServerCapabilities::CompletionItem)
	JSON_TO_VALUE_PROPERTY(labelDetailsSupport)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::CompletionServerCapabilities)
	JSON_TO_VALUE_PROPERTY(triggerCharacters)
	JSON_TO_VALUE_PROPERTY(allCommitCharacters)
	JSON_TO_VALUE_PROPERTY(completionItem)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::CompletionRequest::CompletionRequest::Context)
	JSON_FROM_VALUE_PROPERTY(triggerKind)
	JSON_FROM_VALUE_PROPERTY(triggerCharacter)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::CompletionRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
	JSON_FROM_VALUE_PROPERTY(context)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::CompletionResponse::Item)
	JSON_TO_VALUE_PROPERTY(label)
	JSON_TO_VALUE_PROPERTY(kind)
	JSON_TO_VALUE_PROPERTY(detail)
	JSON_TO_VALUE_PROPERTY(documentation)
	JSON_TO_VALUE_PROPERTY(deprecated)
	JSON_TO_VALUE_PROPERTY(preselect)
	JSON_TO_VALUE_PROPERTY(insertText)
	JSON_TO_VALUE_PROPERTY(filterText)
	JSON_TO_VALUE_PROPERTY(textEdit)
	JSON_TO_VALUE_PROPERTY_NAMED("score", clangdScore)
	//JSON_TO_VALUE_PROPERTY(additionalTextEdits)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::CompletionResponse)
	JSON_TO_VALUE_PROPERTY(isIncomplete)
	JSON_TO_VALUE_PROPERTY(items)
JSON_TO_VALUE_END


// Hover

JSON_FROM_VALUE_BEGIN(Lsp::HoverClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(contentFormat)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::HoverServerCapabilities)
	JSON_TO_VALUE_PROPERTY(workDoneProgress)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::HoverRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::HoverResponse)
	JSON_TO_VALUE_PROPERTY(contents)
	JSON_TO_VALUE_PROPERTY(range)
JSON_TO_VALUE_END


// Document Symbol

JSON_FROM_VALUE_BEGIN(Lsp::DocumentSymbolClientCapabilities::SymbolKinds)
	JSON_FROM_VALUE_PROPERTY(valueSet)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DocumentSymbolClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(symbolKind)
	JSON_FROM_VALUE_PROPERTY(hierarchicalDocumentSymbolSupport)
	JSON_FROM_VALUE_PROPERTY(tagSupport)
	JSON_FROM_VALUE_PROPERTY(labelSupport)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::DocumentSymbolServerCapabilities)
	JSON_TO_VALUE_PROPERTY(label)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DocumentSymbolRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::SymbolInformation)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(kind)
	JSON_TO_VALUE_PROPERTY(deprecated)
	JSON_TO_VALUE_PROPERTY(location)
	JSON_TO_VALUE_PROPERTY(containerName)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::DocumentSymbolResponse)
	if (!JsonToValue(parentTrace, json, &result->symbols))
		return false;
JSON_TO_VALUE_END

// Signature Help

JSON_FROM_VALUE_BEGIN(Lsp::SignatureHelpClientCapabilities::SignatureInformation::ParameterInformation)
	JSON_FROM_VALUE_PROPERTY(labelOffsetSupport)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::SignatureHelpClientCapabilities::SignatureInformation)
	JSON_FROM_VALUE_PROPERTY(parameterInformation)
	JSON_FROM_VALUE_PROPERTY(documentationFormat)
	JSON_FROM_VALUE_PROPERTY(activeParameterSupport)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::SignatureHelpClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(signatureInformation)
	JSON_FROM_VALUE_PROPERTY(contextSupport)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::SignatureHelpServerCapabilities)
	JSON_TO_VALUE_PROPERTY(triggerCharacters)
	JSON_TO_VALUE_PROPERTY(retriggerCharacters)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::SignatureHelpRequest::Context)
	JSON_FROM_VALUE_PROPERTY(triggerKind)
	JSON_FROM_VALUE_PROPERTY(triggerCharacter)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::SignatureHelpRequest)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(position)
	JSON_FROM_VALUE_PROPERTY(context)
JSON_FROM_VALUE_END

bool JsonToValue(const JsonTrace* parentTrace, const cJSON* json, Lsp::SignatureHelpResponse::Signature::Parameter::Label* result) {

	if (cJSON_IsString(json)) {
		result->isSubstring = false;
		result->string = cJSON_GetStringValue(json);
		return true;

	} else if (cJSON_IsArray(json)) {
		result->isSubstring = true;
		return JsonToValue(parentTrace, json, &result->substring);
		 
	} else {
		JsonLogError(parentTrace, "expected value to be either an [array] or a [string] but was [%]", JsonTypeToString(json->type));
		return false;
	}
}

JSON_TO_VALUE_BEGIN(Lsp::SignatureHelpResponse::Signature::Parameter)
	JSON_TO_VALUE_PROPERTY(label)
	JSON_TO_VALUE_PROPERTY(documentation)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::SignatureHelpResponse::Signature)
	JSON_TO_VALUE_PROPERTY(label)
	JSON_TO_VALUE_PROPERTY(documentation)
	JSON_TO_VALUE_PROPERTY(parameters)
	JSON_TO_VALUE_PROPERTY(activeParameter)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::SignatureHelpResponse)
	JSON_TO_VALUE_PROPERTY(signatures)
	JSON_TO_VALUE_PROPERTY(activeSignature)
	JSON_TO_VALUE_PROPERTY(activeParameter)
JSON_TO_VALUE_END

//==============================================================================
// DOCUMENT SYNCHRONIZATION
//==============================================================================

JSON_FROM_VALUE_BEGIN(Lsp::TextDocumentSyncClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(willSave)
	JSON_FROM_VALUE_PROPERTY(willSaveWaitUntil)
	JSON_FROM_VALUE_PROPERTY(didSave)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::TextDocumentSyncServerCapabilities)
	JSON_TO_VALUE_PROPERTY(openClose)
	JSON_TO_VALUE_PROPERTY(change)
	JSON_TO_VALUE_PROPERTY(willSave)
	JSON_TO_VALUE_PROPERTY(save)
JSON_TO_VALUE_END

// JSON_TO_ENUM_BEGIN(Lsp::TextDocumentSyncServerCapabilities::SyncKind)
// 	JSON_TO_ENUM_MEMBER(SyncKind_None, "none")
// 	JSON_TO_ENUM_MEMBER(SyncKind_Full, "full")
// 	JSON_TO_ENUM_MEMBER(SyncKind_Incremental, "incremental")
// JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::TextDocumentSyncServerCapabilities::SaveOptions)
	JSON_TO_VALUE_PROPERTY(includeText)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::TextDocumentItem)
	JSON_FROM_VALUE_PROPERTY(uri)
	JSON_FROM_VALUE_PROPERTY(languageId)
	JSON_FROM_VALUE_PROPERTY(version)
	JSON_FROM_VALUE_PROPERTY(text)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DidOpenTextDocumentNotification)
	JSON_FROM_VALUE_PROPERTY(textDocument)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DidCloseTextDocumentNotification)
	JSON_FROM_VALUE_PROPERTY(textDocument)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DidChangeTextDocumentNotification::ChangeEvent)
	JSON_FROM_VALUE_PROPERTY(range)
	JSON_FROM_VALUE_PROPERTY(text)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::DidChangeTextDocumentNotification)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(contentChanges)
	JSON_FROM_VALUE_PROPERTY_NAMED("wantDiagnostics", clangdWantDiagnostics)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::WillSaveTextDocumentNotification)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(saveReason)
JSON_FROM_VALUE_END

JSON_FROM_ENUM_BEGIN(Lsp::WillSaveTextDocumentNotification::SaveReason)
	JSON_FROM_ENUM_MEMBER("manual", SaveReason_Manual)
	JSON_FROM_ENUM_MEMBER("afterDelay", SaveReason_AfterDelay)
	JSON_FROM_ENUM_MEMBER("focusOut", SaveReason_FocusOut)
JSON_FROM_ENUM_END

JSON_FROM_VALUE_BEGIN(Lsp::DidSaveTextDocumentNotification)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(text)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::PublishDiagnosticsClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(relatedInforamtion)
	JSON_FROM_VALUE_PROPERTY(versionSupport)
	JSON_FROM_VALUE_PROPERTY(codeDescriptionSupport)
	JSON_FROM_VALUE_PROPERTY(dataSupport)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::Diagnostic)
	JSON_TO_VALUE_PROPERTY(range)
	JSON_TO_VALUE_PROPERTY(severity)
	JSON_TO_VALUE_PROPERTY(code)
	JSON_TO_VALUE_PROPERTY(message)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::PublishDiagnosticsNotification)
	JSON_TO_VALUE_PROPERTY(uri)
	JSON_TO_VALUE_PROPERTY(version)
	JSON_TO_VALUE_PROPERTY(diagnostics)
JSON_TO_VALUE_END

//==============================================================================
// Window Features
//==============================================================================

#define TraceValue_Properties(X)\
	X("off", TraceValue_Off)\
	X("messages", TraceValue_Messages)\
	X("verbose", TraceValue_Verbose)

JSON_TO_ENUM_BEGIN(Lsp::TraceValue)
	TraceValue_Properties(JSON_TO_ENUM_MEMBER)
JSON_TO_ENUM_END

JSON_FROM_ENUM_BEGIN(Lsp::TraceValue)
	TraceValue_Properties(JSON_FROM_ENUM_MEMBER)
JSON_FROM_ENUM_END

JSON_FROM_VALUE_BEGIN(Lsp::ShowMessageRequestClientCapabilities::MessageActionItem)
	JSON_FROM_VALUE_PROPERTY(additionalPropertiesSupport)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::ShowMessageRequestClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(messageActionItem)
JSON_FROM_VALUE_END

JSON_TO_ENUM_BEGIN(Lsp::MessageNotification::MessageType)
	JSON_TO_ENUM_MEMBER("unspecified", MessageType_Unspecified)
	JSON_TO_ENUM_MEMBER("error", MessageType_Error)
	JSON_TO_ENUM_MEMBER("warning", MessageType_Warning)
	JSON_TO_ENUM_MEMBER("info", MessageType_Info)
	JSON_TO_ENUM_MEMBER("log", MessageType_Log)
JSON_TO_ENUM_END

JSON_TO_VALUE_BEGIN(Lsp::MessageNotification)
	JSON_TO_VALUE_PROPERTY(type)
	JSON_TO_VALUE_PROPERTY(message)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::LogTraceNotification)
	JSON_TO_VALUE_PROPERTY(message)
	JSON_TO_VALUE_PROPERTY(verbose)
JSON_TO_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::SetTraceParams)
	JSON_FROM_VALUE_PROPERTY(value)
JSON_FROM_VALUE_END


//==============================================================================
// Lifecycle
//==============================================================================

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest::ClientInfo)
	JSON_FROM_VALUE_PROPERTY(name)
	JSON_FROM_VALUE_PROPERTY(version)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest::ClientCapabilities::TextDocumentClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(synchronization)
	JSON_FROM_VALUE_PROPERTY(completion)
	JSON_FROM_VALUE_PROPERTY(hover)
	JSON_FROM_VALUE_PROPERTY(declaration)
	JSON_FROM_VALUE_PROPERTY(definition)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest::ClientCapabilities::General)
	JSON_FROM_VALUE_PROPERTY(positionEncodings)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest::ClientCapabilities::Window)
	JSON_FROM_VALUE_PROPERTY(workDoneProgress)
	JSON_FROM_VALUE_PROPERTY(showMessage)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest::ClientCapabilities)
	JSON_FROM_VALUE_PROPERTY(textDocument)
	JSON_FROM_VALUE_PROPERTY(window)
	JSON_FROM_VALUE_PROPERTY(general)
JSON_FROM_VALUE_END

JSON_FROM_VALUE_BEGIN(Lsp::InitializeRequest)
	JSON_FROM_VALUE_PROPERTY(processId)
	JSON_FROM_VALUE_PROPERTY(clientInfo)
	JSON_FROM_VALUE_PROPERTY(locale)
	JSON_FROM_VALUE_PROPERTY(rootPath)
	JSON_FROM_VALUE_PROPERTY(capabilities)
	JSON_FROM_VALUE_PROPERTY_NAMED("offsetEncoding", clangdOffsetEncoding)
	JSON_FROM_VALUE_PROPERTY(trace)
JSON_FROM_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::InitializeResponse::ServerInfo)
	JSON_TO_VALUE_PROPERTY(name)
	JSON_TO_VALUE_PROPERTY(version)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::InitializeResponse::ServerCapabilities)
	JSON_TO_VALUE_PROPERTY(positionEncoding)
	JSON_TO_VALUE_PROPERTY(textDocumentSync)
	JSON_TO_VALUE_PROPERTY(completionProvider)
	JSON_TO_VALUE_PROPERTY(hoverProvider)
	JSON_TO_VALUE_PROPERTY(signatureHelpProvider)
	JSON_TO_VALUE_PROPERTY(declarationProvider)
	JSON_TO_VALUE_PROPERTY(definitionProvider)
	JSON_TO_VALUE_PROPERTY(typeDefinitionProvider)
	JSON_TO_VALUE_PROPERTY(implementationProvider)
	JSON_TO_VALUE_PROPERTY(referencesProvider)
JSON_TO_VALUE_END

JSON_TO_VALUE_BEGIN(Lsp::InitializeResponse)
	JSON_TO_VALUE_PROPERTY(capabilities)
	JSON_TO_VALUE_PROPERTY(serverInfo)
JSON_TO_VALUE_END

cJSON* JsonFromValue(const Lsp::InitializedNotification&) { return cJSON_CreateNull(); }
cJSON* JsonFromValue(const Lsp::ShutdownRequest&) { return cJSON_CreateNull(); }
bool   JsonToValue(const JsonTrace* trace, const cJSON *json, Lsp::ShutdownResponse*) { return true; }
cJSON* JsonFromValue(const Lsp::ExitNotification&) { return cJSON_CreateNull(); }

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* static FormatArgument F(const Lsp::ErrorResponse* err) {
	return FormatArgument {
		.userptr = err,
		.Write = [] (const FormatArgument* self, std::ostream* target) {
			auto error = static_cast<const Lsp::ErrorResponse*>(self->userptr);
			
			std::string_view knwonCode {};
			switch (error->code) {
				case Lsp::ErrorResponse::Code_ClientParseError: knwonCode = "(ClientParseError)"; break;
				case Lsp::ErrorResponse::Code_ClientInconclusiveMessage: knwonCode = "(ClientInconclusiveMessage)"; break;
				case Lsp::ErrorResponse::Code_ParseError: knwonCode = "(ParseError)"; break;
				case Lsp::ErrorResponse::Code_InvalidRequest: knwonCode = "(InvalidRequest)"; break;
				case Lsp::ErrorResponse::Code_MethodNotFound: knwonCode = "(MethodNotFound)"; break;
				case Lsp::ErrorResponse::Code_InvalidParams: knwonCode = "(InvalidParams)"; break;
				case Lsp::ErrorResponse::Code_InternalError: knwonCode = "(InternalError)"; break;
				case Lsp::ErrorResponse::Code_ServerNotInitialized: knwonCode = "(ServerNotInitialized)"; break;
				case Lsp::ErrorResponse::Code_UnknownErrorCode: knwonCode = "(UnknownErrorCode)"; break;
				case Lsp::ErrorResponse::Code_RequestFailed: knwonCode = "(RequestFailed)"; break;
				case Lsp::ErrorResponse::Code_ServerCancelled: knwonCode = "(ServerCancelled)"; break;
				case Lsp::ErrorResponse::Code_ContentModified: knwonCode = "(ContentModified)"; break;
				case Lsp::ErrorResponse::Code_RequestCancelled: knwonCode = "(RequestCancelled)"; break;
				default: knwonCode = ""; break;
			}
			
			(*target) << error->code << knwonCode << error->message;
		}};
}*/

