#pragma once
#include "basic.hh"
#include "util/format.hh"

#include <string>
#include <vector>
#include <optional>
#include <array>

struct cJSON;

namespace LanguageServerProtocol {

	//=============================================================================
	// CONSTANTS
	//=============================================================================

	constexpr std::string_view HEADER_CONTENT_LENGTH = "Content-Length: ";
	constexpr std::string_view HEADER_CONTENT_TYPE = "Content-Type: ";
	constexpr std::string_view HEADER_DELIMITER = "\r\n";
	constexpr std::string_view HEADER_START_PAYLOAD = "\r\n\r\n";
	
	constexpr std::string_view JSONRPC_VERSION = "jsonrpc";
	constexpr std::string_view JSONRPC_ID = "id";
	constexpr std::string_view JSONRPC_RESULT = "result";
	constexpr std::string_view JSONRPC_ERROR = "error";
	constexpr std::string_view JSONRPC_PARAMS = "params";
	constexpr std::string_view JSONRPC_METHOD = "method";

	//=============================================================================
	// MESSAGES
	//=============================================================================
	
	struct Header {
		u64 contentLength = 0u;
		std::string_view contentType = {};
	};

	struct ErrorResponse {
		
		enum Code {
		
			// NOTE: not part of the official sepcs
			Code_Success = 0,
			
			// these errors are defined by us. they indicate that something in the lsp-server-code went wrong.
			// See the ProcessMessage()-function
			Code_ClientParseError = -1,
			Code_ClientInconclusiveMessage = -2,

			// Defined by JSON-RPC
			Code_ParseError = -32700,
			Code_InvalidRequest = -32600,
			Code_MethodNotFound = -32601,
			Code_InvalidParams = -32602,
			Code_InternalError = -32603,

			Code_ServerNotInitialized = -32002,
			Code_UnknownErrorCode = -32001,
			Code_RequestFailed = -32803,
			Code_ServerCancelled = -32802,
			Code_ContentModified = -32801,
			Code_RequestCancelled = -32800,
		};

		// NOTE: should be an integer
		s64 code = Code_Success;
		std::string_view message = {};
		cJSON *data = nullptr;
	};

	//=============================================================================
	// GERNERAL STRUCTURES
	//=============================================================================

	struct Position {
		u64 line = 0;  // zero-based!
		u64 character = 0; // zero-based, according to PositionEncodingKind
	};

	struct Range {
		Position start = {};
		Position end = {};
	};

	struct Location {
		std::string_view uri = {};
		Range range = {};
	};
	
	enum PositionEncodingKind {
		PositionEncodingKind_Undefined = 0, // NOTE: not part of the offical spec
		PositionEncodingKind_Utf8,
		PositionEncodingKind_Utf16,
		PositionEncodingKind_Utf32
	};

	// @FIXME typo
	struct TextDocumentIdentifier {
		std::string_view uri = {};
	};

	struct VersionedTextDocumentIdentifier {
		std::string_view uri = {};
		s32 version = 0; // will increase after each change, doesn't need to be consecutive.
	};

	enum MarkupKind {
		MarkupKind_Plaintext,
		MarkupKind_Markdown
	};

	struct MarkupString {
		MarkupKind kind = MarkupKind_Plaintext;
		std::string language = {};
		std::string value = {};
	};

	struct TextEdit {
		// To insert text into a document create a range where start == end.
		// For delete operations use an empty string.
		
		Range range = {};
		std::string_view newText = {};
	};
	
	struct Response {
		virtual ~Response() {}
	};

	//=============================================================================
	// LANGUAGE FEATURES
	//=============================================================================

	//-----------------------------------------------------------------------------
	// Goto
	//
	// The go to decleration/definition/implementation/references etc. request is sent from 
	// the client to the server to resolve the declaration location of a symbol at
	// a given text document position.
	//-----------------------------------------------------------------------------

	struct GotoClientCapabilities {
		bool linkSupport = true;
	};

	struct GotoServerCapabilities {
		bool workDoneProgress = false;
	};

	struct GotoDeclerationRequest {
		static constexpr std::string_view METHOD = "textDocument/declaration";
		TextDocumentIdentifier textDocument = {};
		Position position = {};
	};
	struct GotoDefinitionRequest {
		static constexpr std::string_view METHOD = "textDocument/definition";
		TextDocumentIdentifier textDocument = {};
		Position position = {};
	};
	struct GotoTypeDefinitionRequest {
		static constexpr std::string_view METHOD = "textDocument/typeDefinition";
		TextDocumentIdentifier textDocument = {};
		Position position = {};
	};
	struct GotoImplementationRequest {
		static constexpr std::string_view METHOD = "textDocument/implementation";
		TextDocumentIdentifier textDocument = {};
		Position position = {};
	};

	struct LocationLink {
		Range originSelectionRange = {};
		std::string_view targetUri = {};
		Range targetRange = {};
		Range targetSelectionRange = {};
		
		// NOTE: not part of the offical spec
		// We implemented the FromJson-Value so that it can handle both Location and
		// LocationLink-Responses, because the Goto-Requests are allowed to answer
		// with both variants.
		// isSimpleLocation=true indicates that the response was just a Location and
		// thus the targetRange and originSelectionRange-Members are not set
		bool isSimpleLocation = false;
	};
		
	struct GotoResponse : public Response {
		std::vector<LocationLink> locations = {};
	};
	
	// The "Find References" has no link support, always responds with a Location[]-array
	// and has an additional context in the request.
	// the rest is similar enough that we keep it in the goto-section
	struct GotoReferencesClientCapabilities {
	};
	
	struct GotoReferencesRequest {
		static constexpr std::string_view METHOD_REFERENCES = "textDocument/references";
		
		struct ReferenceContext {
			bool includeDecleration = false;
		};
		
		TextDocumentIdentifier textDocument = {};
		Position position = {};
		ReferenceContext context = {};
	};
	
	//-----------------------------------------------------------------------------
	// Completion Request
	//
	// The Completion request is sent from the client to the server to compute
	// completion items at a given cursor position. Completion items are presented
	// in the IntelliSense user interface. If computing full completion items is
	// expensive, servers can additionally provide a handler for the completion
	// item resolve request ('completionItem/resolve'). This request is sent when
	// a completion item is selected in the user interface. A typical use case is
	// for example: the textDocument/completion request doesn't fill in the
	// documentation property for returned completion items since it is expensive
	// to compute. When the item is selected in the user interface then a
	// 'completionItem/resolve' request is sent with the selected completion item
	// as a parameter. The returned completion item should have the documentation
	// property filled in. By default the request can only delay the computation of
	//  the detail and documentation properties.
	//-----------------------------------------------------------------------------

	enum InsertTextMode {
		 InsertTextMode_AsIs = 1,
		 InsertTextMode_AdjustIndentation = 2
	};

	enum CompletionItemKind {
  	     CompletionItemKind_Unspecified = 0, // NOTE: not part of the offical spec
		 CompletionItemKind_Text = 1,
		 CompletionItemKind_Method = 2,
		 CompletionItemKind_Function = 3,
		 CompletionItemKind_Constructor = 4,
		 CompletionItemKind_Field = 5,
		 CompletionItemKind_Variable = 6,
		 CompletionItemKind_Class = 7,
		 CompletionItemKind_Interface = 8,
		 CompletionItemKind_Module = 9,
		 CompletionItemKind_Property = 10,
		 CompletionItemKind_Unit = 11,
		 CompletionItemKind_Value = 12,
		 CompletionItemKind_Enum = 13,
		 CompletionItemKind_Keyword = 14,
		 CompletionItemKind_Snippet = 15,
		 CompletionItemKind_Color = 16,
		 CompletionItemKind_File = 17,
		 CompletionItemKind_Reference = 18,
		 CompletionItemKind_Folder = 19,
		 CompletionItemKind_EnumMember = 20,
		 CompletionItemKind_Constant = 21,
		 CompletionItemKind_Struct = 22,
		 CompletionItemKind_Event = 23,
		 CompletionItemKind_Operator = 24,
		 CompletionItemKind_TypeParameter = 25,
		 CompletionItemKind_TypeParameter_MAX
	};

	struct CompletionClientCapabilities {

		struct CompletionItem {
			bool snippetSupport = false;
			bool commitCharactersSupport = false;
			std::vector<MarkupKind> documentationFormat = {MarkupKind_Plaintext};
			bool deprecatedSupport = true;
			bool preselectSupport = false;
			bool insertReplaceSupport = false;
			bool labelDetailsSupport = false;
		};

		struct ItemKinds {
			std::array<int, CompletionItemKind_TypeParameter_MAX> valueSet = {
				CompletionItemKind_Text, CompletionItemKind_Method, CompletionItemKind_Function, CompletionItemKind_Constructor, CompletionItemKind_Field, CompletionItemKind_Variable, CompletionItemKind_Class, CompletionItemKind_Interface, CompletionItemKind_Module, CompletionItemKind_Property, CompletionItemKind_Unit, CompletionItemKind_Value, CompletionItemKind_Enum, CompletionItemKind_Keyword, CompletionItemKind_Snippet, CompletionItemKind_Color, CompletionItemKind_File, CompletionItemKind_Reference, CompletionItemKind_Folder, CompletionItemKind_EnumMember, CompletionItemKind_Constant, CompletionItemKind_Struct, CompletionItemKind_Event, CompletionItemKind_Operator, CompletionItemKind_TypeParameter };
		};

		CompletionItem completionItem = {};
		ItemKinds completionItemKind = {};
		bool contextSupport = true;

		int inserTextMode = InsertTextMode_AsIs;
	};

	struct CompletionServerCapabilities {
		
		std::vector<std::string> triggerCharacters = {};
		std::vector<std::string> allCommitCharacters = {};
		bool resolveProvider = false;

		struct CompletionItem {
			bool labelDetailsSupport = false;
		} completionItem;
	};

	struct CompletionRequest {

		static constexpr std::string_view METHOD = "textDocument/completion";
		
		struct Context {
			
			enum TriggerKind {
				 TriggerKind_Unknown = 0, // NOTE: not part of the offical spec
				 TriggerKind_Invoked = 1,
				 TriggerKind_TriggerCharacter = 2,
				 TriggerKind_TriggerForIncompleteCompletions = 3
			};
			
			int  triggerKind = TriggerKind_Unknown;
			std::string triggerCharacter = {};
		};

		TextDocumentIdentifier textDocument = {};
		Position position = {};
		Context context = {};
	};

	struct CompletionResponse : public Response {

		struct Item {
			
			// The label property is also by default the text that
			// is inserted when selecting this completion.
			//
			// If label details are provided the label itself should
			// be an unqualified name of the completion item.
			std::string_view label = {};
			
			int kind = CompletionItemKind_Unspecified;

			std::string_view detail = {};

			MarkupString documentation = {};
			
			bool deprecated = true;
			
			bool preselect = false;
			
			std::string_view insertText = {};

			std::string_view filterText = {};
			
			TextEdit textEdit = {};

			float clangdScore = 0.0f;
			//std::vector<TextEdit> additionalTextEdits = {};
		};

		bool isIncomplete = false;
		std::vector<Item> items = {};
	};

	//-----------------------------------------------------------------------------
	// Hover Request
	//
	// The hover request is sent from the client to the server to request hover 
	// information at a given text document position.
	//-----------------------------------------------------------------------------

	struct HoverClientCapabilities {
		std::vector<MarkupKind> contentFormat = {MarkupKind_Plaintext};
	};

	struct HoverServerCapabilities {
		bool workDoneProgress = false;
	};

	struct HoverRequest {

		static constexpr std::string_view METHOD = "textDocument/hover";
				
		TextDocumentIdentifier textDocument = {};
		Position position = {};
	};

	struct HoverResponse : public Response {

		MarkupString contents = {};

		// An optional range is a range inside a text document
		// that is used to visualize a hover, e.g. by changing the background color.
		Range range = {};
	};
	
	//----------------------------------------------------------------------------- 
	// Document Symbols Request
	//
	// The document symbol request is sent from the client to the server.
	// The returned result is either
	// * SymbolInformation[] which is a flat list of all symbols found in a given
	//   text document. Then neither the symbols location range nor the symbols
	//   container name should be used to infer a hierarchy.
	// * DocumentSymbol[] which is a hierarchy of symbols found in a given text
	//   document.
	//----------------------------------------------------------------------------- 

	enum SymbolInformationKind {
		SymbolInformationKind_Unknown = 0, // NOTE: not part of the offical spec
		SymbolInformationKind_File = 1,
		SymbolInformationKind_Module = 2,
		SymbolInformationKind_Namespace = 3,
		SymbolInformationKind_Package = 4,
		SymbolInformationKind_Class = 5,
		SymbolInformationKind_Method = 6,
		SymbolInformationKind_Property = 7,
		SymbolInformationKind_Field = 8,
		SymbolInformationKind_Constructor = 9,
		SymbolInformationKind_Enum = 10,
		SymbolInformationKind_Interface = 11,
		SymbolInformationKind_Function = 12,
		SymbolInformationKind_Variable = 13,
		SymbolInformationKind_Constant = 14,
		SymbolInformationKind_String = 15,
		SymbolInformationKind_Number = 16,
		SymbolInformationKind_Boolean = 17,
		SymbolInformationKind_Array = 18,
		SymbolInformationKind_Object = 19,
		SymbolInformationKind_Key = 20,
		SymbolInformationKind_Null = 21,
		SymbolInformationKind_EnumMember = 22,
		SymbolInformationKind_Struct = 23,
		SymbolInformationKind_Event = 24,
		SymbolInformationKind_Operator = 25,
		SymbolInformationKind_TypeParameter = 26,
		SymbolInformationKind_MAX
	};

	struct DocumentSymbolClientCapabilities {
		
		struct SymbolKinds {
			std::array<int, SymbolInformationKind_MAX> valueSet = {
				SymbolInformationKind_File, SymbolInformationKind_Module, SymbolInformationKind_Namespace, SymbolInformationKind_Package, SymbolInformationKind_Class, SymbolInformationKind_Method, SymbolInformationKind_Property, SymbolInformationKind_Field, SymbolInformationKind_Constructor, SymbolInformationKind_Enum, SymbolInformationKind_Interface, SymbolInformationKind_Function, SymbolInformationKind_Variable, SymbolInformationKind_Constant, SymbolInformationKind_String, SymbolInformationKind_Number, SymbolInformationKind_Boolean, SymbolInformationKind_Array, SymbolInformationKind_Object, SymbolInformationKind_Key, SymbolInformationKind_Null, SymbolInformationKind_EnumMember, SymbolInformationKind_Struct, SymbolInformationKind_Event, SymbolInformationKind_Operator, SymbolInformationKind_TypeParameter
			};
		};
		
		SymbolKinds symbolKind = {};
		
		bool hierarchicalDocumentSymbolSupport = false;
		bool tagSupport = false;
		bool labelSupport = true; // @TODO(outline-label)
	};
	
	struct DocumentSymbolServerCapabilities {
		std::string_view label = {};
	};
	
	struct DocumentSymbolRequest {
		static constexpr std::string_view METHOD = "textDocument/documentSymbol";
		
		TextDocumentIdentifier textDocument = {};
	};

	struct SymbolInformation {
		std::string_view name = {};
		int kind = SymbolInformationKind_Unknown;
		bool deprecated = false;
		Location location = {};
		std::string_view containerName = {};
	};
	
	struct DocumentSymbolResponse : public Response {
		std::vector<SymbolInformation> symbols = {};
	};

	//-----------------------------------------------------------------------------
	// Diagnostics
	//
	// Diagnostics notifications are sent from the server to the client to signal
	// results of validation runs.
	// Diagnostics are "owned" by the server so it is the serve's responsibility
	// to clear them if necessary.
	//-----------------------------------------------------------------------------
	
	struct PublishDiagnosticsClientCapabilities {
		bool relatedInforamtion = false;
		bool versionSupport = true;
		bool codeDescriptionSupport = false;
		bool dataSupport = false;
	};
	
	struct Diagnostic {
		enum Severity {
			Severity_Unknown = 0, // NOTE not part of the official spec
			Severity_Error = 1,
			Severity_Warning = 2,
			Severity_Information = 3,
			Severity_Hint = 4
		};
		
		Range range = {};
		int severity = Severity_Unknown;
		std::string_view code = {};
		std::string_view message = {};
		
		// @TODO
		// std::vector<DiagnosticsRelatedInformation> relatedInformation = {};
	};
	
	struct PublishDiagnosticsNotification {
		static constexpr std::string_view METHOD = "textDocument/publishDiagnostics";
	
		std::string_view uri = {};
		int version = 0;
		std::vector<Diagnostic> diagnostics = {};
	};

	//-----------------------------------------------------------------------------
	// Signature Help
 	//
	// The signature help request is sent from the client to the server to request
	// signature information at a given cursor position.
	//-----------------------------------------------------------------------------

	struct SignatureHelpClientCapabilities {
		
		struct SignatureInformation {
			
			struct ParameterInformation {
				bool labelOffsetSupport = true;
			} parameterInformation = {};

			std::vector<MarkupKind> documentationFormat = {MarkupKind_Plaintext};
			bool activeParameterSupport = true;
		
		} signatureInformation = {};
		bool contextSupport = true;
	};

	struct SignatureHelpServerCapabilities {
		std::vector<std::string> triggerCharacters = {};
		std::vector<std::string> retriggerCharacters = {};
	};

	struct SignatureHelpRequest {
		static constexpr std::string_view METHOD = "textDocument/signatureHelp";
		
		struct Context {
			enum TriggerKind {
				 TriggerKind_Unknown = 0, // NOTE: not part of the offical spec
				 TriggerKind_Invoked = 1,
				 TriggerKind_TriggerCharacter = 2,
				 TriggerKind_ContentChange = 3
			};
			
			int triggerKind = TriggerKind_Unknown;
			std::string triggerCharacter = {};
		};

		TextDocumentIdentifier textDocument = {};
		Position position = {};
		Context context = {};
	};
	
	struct SignatureHelpResponse : public Response {

		struct Signature {

			struct Parameter {

				// can be either string or [uint, uint]
				struct Label {
					bool isSubstring = false;
					union {
						std::string_view string;
						std::array<int, 2> substring;
					};
				};
				
				Label label = {}; 
				MarkupString documentation = {};
			};
			
			std::string label = {};
			MarkupString documentation = {};
			std::vector<Parameter> parameters = {};
			int activeParameter = -1;
		};

		std::vector<Signature> signatures = {};
		int activeSignature = -1;
		int activeParameter = -1;
	};

	//==============================================================================
	// DOCUMENT SYNCHRONIZATION
	//==============================================================================

	struct TextDocumentSyncClientCapabilities {

		bool willSave = true;
		bool willSaveWaitUntil = false;
		bool didSave = true;
	};

	struct TextDocumentSyncServerCapabilities {

		bool openClose = false;

		enum SyncKind {
			 SyncKind_None = 0,
			 SyncKind_Full = 1,
			 SyncKind_Incremental = 2
		
		};

		// NOTE: should be an integer
		int change = SyncKind_None;
		
		bool willSave = false;

		// bool willSaveWaitUntil =false;

		struct SaveOptions {
			bool includeText = false;
		};
		
		std::optional<SaveOptions> save = {};
	};

	struct TextDocumentItem {

		std::string_view uri = {};
		std::string_view languageId = {};
		
		// The version number of this document (it will increase after each
		// change, including undo/redo).
		s64 version = 0;

		std::string_view text = {};
	};

	//-----------------------------------------------------------------------------
	// DidOpen/CloseTextDocument Notification
	// 
	// The document open notification is sent from the client to the server to 
	// signal newly opened text documents. The document's content is now managed by
	// the client and the server must not try to read the document's content using
	// the document's Uri. Open in this sense means it is managed by the client.
	// It doesn't necessarily mean that its content is presented in an editor. An
	// open notification must not be sent more than once without a corresponding
	// close notification send before. This means open and close notification must
	// be balanced and the max open count for a particular textDocument is one.
	// Note that a server's ability to fulfill requests is independent of whether
	// a text document is open or closed.
	//-----------------------------------------------------------------------------

	struct DidOpenTextDocumentNotification {
		static constexpr std::string_view METHOD = "textDocument/didOpen";
		
		TextDocumentItem textDocument = {};
	};

	struct DidCloseTextDocumentNotification {
		static constexpr std::string_view METHOD = "textDocument/didClose";
		
		TextDocumentIdentifier textDocument = {};
	};

	//-----------------------------------------------------------------------------
	// DidChangeTextDocument Notification
	//
	// The document change notification is sent from the client to the server to
	// signal changes to a text document. Before a client can change a text
	// document it must claim ownership of its content using the
	// textDocument/didOpen notification. In 2.0 the shape of the params has
	// changed to include proper version numbers.
	//-----------------------------------------------------------------------------

	struct DidChangeTextDocumentNotification {
		static constexpr std::string_view METHOD = "textDocument/didChange";
		
		VersionedTextDocumentIdentifier textDocument = {};

		struct ChangeEvent {
			std::optional<Range> range = std::nullopt; 	// ommited if syncKind == SyncKind_Full
			
			std::string_view text = {};	// the new text for the range or the full document text if syncKind == SyncKind_Full
		};

		std::vector<ChangeEvent> contentChanges = {};

		std::optional<bool> clangdWantDiagnostics = std::nullopt;
	};

	//-----------------------------------------------------------------------------
	// WillSaveTextNotification
	//
	// The document will save notification is sent from the client to the server
	// before the document is actually saved. If a server has registered for
	// open / close events clients should ensure that the document is open before a
	// willSave notification is sent since clients can't change the content of a
	// file without ownership transferal.
	//-----------------------------------------------------------------------------

	struct WillSaveTextDocumentNotification {
		static constexpr std::string_view METHOD = "textDocument/willChange";
		
		TextDocumentIdentifier textDocument = {};

		enum SaveReason {
			 SaveReason_Manual = 1,
			 SaveReason_AfterDelay = 2,
			 SaveReason_FocusOut = 3,	
		} saveReason = SaveReason_Manual;
	};

	struct DidSaveTextDocumentNotification {
		static constexpr std::string_view METHOD = "textDocument/didSave";
				
		TextDocumentIdentifier textDocument = {};
		std::optional<std::string> text = {};
	};

	//==============================================================================
	// Window Features
	//==============================================================================

	//-----------------------------------------------------------------------------
	// ShowMessage Request
	//
	// The show message notification is sent from a server to a client to ask the
	// client to display a particular message in the user interface.
	//-----------------------------------------------------------------------------

	struct ShowMessageRequestClientCapabilities {

		struct MessageActionItem {
			bool additionalPropertiesSupport = false;
		
		} messageActionItem = {};
	};

	// NOTE: not part of the offical spec 
	// LogMessage- and ShowMessage-Notification have the same layout
	struct MessageNotification {

		static constexpr std::string_view METHOD_SHOW_MESSAGE = "window/showMessage";
		static constexpr std::string_view METHOD_LOG_MESSAGE = "window/logMessage";

		enum MessageType {
			MessageType_Unspecified = 0, // NOTE: not part of the offical spec
			MessageType_Error = 1,
			MessageType_Warning = 2,
			MessageType_Info = 3,
			MessageType_Log = 4
		} type = MessageType_Unspecified;

		std::string message = {};
	};

	//-----------------------------------------------------------------------------
	// LogTrace Notification
	//
	// A notification to log the trace of the server's execution. The amount and
	// content of these notifications depends on the current trace configuration.
	// If trace is 'off', the server should not send any logTrace notification.
	// If trace is 'messages', the server should not add the 'verbose' field in
	// the LogTraceParams.
	//-----------------------------------------------------------------------------

	enum TraceValue {
		TraceValue_Off,
		TraceValue_Messages,
		TraceValue_Verbose
	};

	struct LogTraceNotification {
		static constexpr std::string_view METHOD = "$/logTrace";
		std::string message;
		std::string verbose;
	};

	struct SetTraceParams {
		TraceValue value = TraceValue_Off;
	};

	//==============================================================================
	// Lifecycle
	//==============================================================================

	struct InitializeRequest {
		static constexpr std::string_view METHOD = "initialize";
		
		int processId = 0;

		struct ClientInfo {
			std::string name = {};
			std::string version = {};
		} clientInfo = {};

		// The locale the client is currently showing the user interface
		// in. This must not necessarily be the locale of the operating
		// system.
		//
		// Uses IETF language tags as the value's syntax
		// (See https://en.wikipedia.org/wiki/IETF_language_tag)
		std::string locale = "en";

		std::string rootPath = {};

		// User provided initialization options.
		//initializationOptions?: LSPAny;

		struct ClientCapabilities {

			struct TextDocumentClientCapabilities {

				TextDocumentSyncClientCapabilities synchronization = {};

				CompletionClientCapabilities completion = {};

				HoverClientCapabilities hover = {};

				//signatureHelp?: SignatureHelpClientCapabilities;

				GotoClientCapabilities declaration = {};

				GotoClientCapabilities definition = {};

				GotoClientCapabilities typeDefinition = {};
				
				GotoClientCapabilities implementation = {};
				
				GotoReferencesClientCapabilities references = {};

				PublishDiagnosticsClientCapabilities publishDiagnostics = {};

				//semanticTokens?: SemanticTokensClientCapabilities;

				//diagnostic?: DiagnosticClientCapabilities;
			
			} textDocument = {};

			// Window specific client capabilities.
			struct Window {
				
				bool workDoneProgress = false;
				ShowMessageRequestClientCapabilities showMessage = {};
				//showDocument?: ShowDocumentClientCapabilities;

			} window = {};

			struct General {
				// The position encodings supported by the client. Client and server
				// have to agree on the same position encoding to ensure that offsets
				// (e.g. character position in a line) are interpreted the same on both
				// side.
				std::vector<PositionEncodingKind> positionEncodings = {PositionEncodingKind_Utf16};

			} general = {};

			// Experimental client capabilities.
			//Any experimental;
		
		} capabilities = {};

		// clangd doesn't support 'general.postionEncodings' and has its own 'offsetEncoding' instead
		std::optional<std::vector<PositionEncodingKind>> clangdOffsetEncoding = std::nullopt;

		// The initial trace setting. If omitted trace is disabled ('off').
		TraceValue trace = TraceValue_Off;
	};

	struct InitializeResponse : public Response {

		struct ServerCapabilities {
			// The position encoding the server picked from the encodings offered
			// by the client via the client capability `general.positionEncodings`.
			//
			// If the client didn't provide any position encodings the only valid
			// value that a server can return is 'utf-16'.
			PositionEncodingKind positionEncoding = PositionEncodingKind_Utf16;

			std::optional<TextDocumentSyncServerCapabilities> textDocumentSync = std::nullopt;

			std::optional<CompletionServerCapabilities> completionProvider = std::nullopt;

			std::optional<HoverServerCapabilities> hoverProvider = std::nullopt;
			
			std::optional<SignatureHelpServerCapabilities> signatureHelpProvider = std::nullopt;

			// @TODO diagnostics Capabilites missing
			
			std::optional<GotoServerCapabilities> declarationProvider = std::nullopt;
			
			std::optional<GotoServerCapabilities> definitionProvider = std::nullopt;
			
			std::optional<GotoServerCapabilities> typeDefinitionProvider = std::nullopt;
			
			std::optional<GotoServerCapabilities> implementationProvider = std::nullopt;
			
			std::optional<GotoServerCapabilities> referencesProvider = std::nullopt;

			// Any experimental;
			
		} capabilities = {};

		struct ServerInfo {
			std::string name = {}; 
			std::string version = {};
		} serverInfo = {};
	};

	// NOTE: these are always null

	struct InitializedNotification {
		static constexpr std::string_view METHOD = "initialized";
	};

	struct ShutdownRequest {
		static constexpr std::string_view METHOD = "shutdown";
	};

	struct ShutdownResponse : public Response {
	};
	
	struct ExitNotification {
		static constexpr std::string_view METHOD = "exit";
	};
}

namespace Lsp = LanguageServerProtocol;

//==============================================================================
// JSON Mapping
//==============================================================================

struct JsonTrace;

bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::ErrorResponse* result);

bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::Position* result);
cJSON* JsonFromValue(const Lsp::Position& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::Range* result);
cJSON* JsonFromValue(const Lsp::Range& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::Location* result);
cJSON* JsonFromValue(const Lsp::Location& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::PositionEncodingKind* result);
cJSON* JsonFromValue(const Lsp::PositionEncodingKind& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TextDocumentIdentifier* result);
cJSON* JsonFromValue(const Lsp::TextDocumentIdentifier& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::VersionedTextDocumentIdentifier* result);
cJSON* JsonFromValue(const Lsp::VersionedTextDocumentIdentifier& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::MarkupKind* result);
cJSON* JsonFromValue(const Lsp::MarkupKind& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::MarkupString* result);
cJSON* JsonFromValue(const Lsp::MarkupString& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TextEdit* result);
cJSON* JsonFromValue(const Lsp::TextEdit& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::CompletionItemKind* result);
cJSON* JsonFromValue(const Lsp::CompletionItemKind& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TraceValue* result);
cJSON* JsonFromValue(const Lsp::TraceValue& value);

cJSON* JsonFromValue(const Lsp::GotoClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::GotoServerCapabilities* result);
cJSON* JsonFromValue(const Lsp::GotoDeclerationRequest& value);
cJSON* JsonFromValue(const Lsp::GotoDefinitionRequest& value);
cJSON* JsonFromValue(const Lsp::GotoTypeDefinitionRequest& value);
cJSON* JsonFromValue(const Lsp::GotoImplementationRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::GotoResponse* result);
cJSON* JsonFromValue(const Lsp::GotoReferencesClientCapabilities& value);
cJSON* JsonFromValue(const Lsp::GotoReferencesRequest& value);

cJSON* JsonFromValue(const Lsp::CompletionClientCapabilities::CompletionItem& value);
cJSON* JsonFromValue(const Lsp::CompletionClientCapabilities::ItemKinds& value);
cJSON* JsonFromValue(const Lsp::CompletionClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::CompletionServerCapabilities::CompletionItem* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::CompletionServerCapabilities* result);
cJSON* JsonFromValue(const Lsp::CompletionRequest::CompletionRequest::Context& value);
cJSON* JsonFromValue(const Lsp::CompletionRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::CompletionResponse::Item* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::CompletionResponse* result);

cJSON* JsonFromValue(const Lsp::HoverClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::HoverServerCapabilities* result);
cJSON* JsonFromValue(const Lsp::HoverRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::HoverResponse* result);
  
cJSON* JsonFromValue(const Lsp::DocumentSymbolClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::DocumentSymbolServerCapabilities* result);
cJSON* JsonFromValue(const Lsp::DocumentSymbolRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::SymbolInformation* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::DocumentSymbolResponse* result);

cJSON* JsonFromValue(const Lsp::TextDocumentSyncClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TextDocumentSyncServerCapabilities::SyncKind* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TextDocumentSyncServerCapabilities::SaveOptions* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::TextDocumentSyncServerCapabilities* result);

cJSON* JsonFromValue(const Lsp::TextDocumentItem& value);
cJSON* JsonFromValue(const Lsp::DidOpenTextDocumentNotification& value);
cJSON* JsonFromValue(const Lsp::DidCloseTextDocumentNotification& value);
cJSON* JsonFromValue(const Lsp::DidChangeTextDocumentNotification::ChangeEvent& value);
cJSON* JsonFromValue(const Lsp::DidChangeTextDocumentNotification& value);
cJSON* JsonFromValue(const Lsp::WillSaveTextDocumentNotification::SaveReason& value);
cJSON* JsonFromValue(const Lsp::WillSaveTextDocumentNotification& value);
cJSON* JsonFromValue(const Lsp::DidSaveTextDocumentNotification& value);

cJSON* JsonFromValue(const Lsp::ShowMessageRequestClientCapabilities::MessageActionItem& value);
cJSON* JsonFromValue(const Lsp::ShowMessageRequestClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::MessageNotification::MessageType* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::MessageNotification* result);

cJSON* JsonFromValue(const Lsp::PublishDiagnosticsClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::Diagnostic* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::PublishDiagnosticsNotification* result);

cJSON* JsonFromValue(const Lsp::SignatureHelpClientCapabilities& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::SignatureHelpServerCapabilities* result);
cJSON* JsonFromValue(const Lsp::SignatureHelpRequest& value);
cJSON* JsonFromValue(const Lsp::SignatureHelpRequest::Context& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::SignatureHelpResponse* result);

bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::LogTraceNotification* result);
cJSON* JsonFromValue(const Lsp::SetTraceParams& value);

cJSON* JsonFromValue(const Lsp::InitializeRequest::ClientInfo& value);
cJSON* JsonFromValue(const Lsp::InitializeRequest::ClientCapabilities& value);
cJSON* JsonFromValue(const Lsp::InitializeRequest::ClientCapabilities::TextDocumentClientCapabilities& value);
cJSON* JsonFromValue(const Lsp::InitializeRequest::ClientCapabilities::Window& value);
cJSON* JsonFromValue(const Lsp::InitializeRequest::ClientCapabilities::General& value);
cJSON* JsonFromValue(const Lsp::InitializeRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::InitializeResponse::ServerCapabilities* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::InitializeResponse::ServerInfo* result);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::InitializeResponse* result);
cJSON* JsonFromValue(const Lsp::ShutdownRequest& value);
bool   JsonToValue  (const JsonTrace* trace, const cJSON* json, Lsp::ShutdownResponse* result);
cJSON* JsonFromValue(const Lsp::InitializedNotification& value);
cJSON* JsonFromValue(const Lsp::ExitNotification& value);

// for logging
//FormatArgument F(const Lsp::ErrorResponse* err);


