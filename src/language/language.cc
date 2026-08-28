#include "language.hh"

#include "editor/editor.hh"
#include "editor/editor-diagnostics.hh"
#include "editor/editor-signaturehelp.hh"
#include "editor/editor-autocomplete.hh"
#include "editor/editor-textlocationlist.hh"

#include "app.hh"
#include "util.hh"
#include "logging.hh"
#include "ui/window.hh"

#include <mutex>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::vector<Language*> Language::languages = {};

bool Language::LoadLanguages(std::string_view directory) {
	std::string fileBuffer = {};
	
	DirectoryIterator iter {directory};	
	while (iter.Next()) {

		if (!iter.IsDirectory())
			continue;
		
		char pathBuffer[MAX_PATH] {};
		const int pathLen = sprintf_s(pathBuffer, "%.*s\\%.*s\\language.toml", SIZE_AND_DATA(iter.GetSearchPath()), SIZE_AND_DATA(iter.filename));
		ASSERT(pathLen >= 0)
		
		const std::string_view path {pathBuffer, static_cast<u64>(pathLen)};
		LogTrace("reading language: '%.*s'", SIZE_AND_DATA(path));
		
		fileBuffer.clear();
		if (!ReadEntireFile(path, &fileBuffer))
			continue;
			
		toml::parse_result parseResult = toml::parse(fileBuffer, path);
		if (parseResult.failed()) {
			const toml::parse_error& error = parseResult.error();
			LogError("failed to parse toml %s: %s. Ignoring language...", Str(error.source()), error.description());
			continue;
		}
		
		toml::table& tblLanguage = parseResult.table();
		
		auto language = new Language();
		if (auto valName = tblLanguage.get_as<std::string>("name"))
			language->name = std::move(valName->get());
		else
			language->name = iter.filename;
		
		if (auto arrFileEndings = tblLanguage.get_as<toml::array>("file-endings")) {
			for (toml::node& nodeEnding : *arrFileEndings) {
				if (auto valEnding = nodeEnding.as<std::string>())
					language->fileEndings.push_back(std::move(valEnding->get()));
			}
		}
		
		if (auto nodeLanguageServer = tblLanguage.get_as<toml::table>("language-server")) {
			if (auto valCommand = nodeLanguageServer->get_as<std::string>("command")) {
				language->serverStartInfo.commandLine = valCommand->get();
			} else {
				LogError("%s: expected entry 'command' as string", Str(nodeLanguageServer->source()));
				language->serverStartup = Startup_Never;
				goto skip_startup;
			}
			
			if (auto valStartup = nodeLanguageServer->get_as<std::string>("startup")) {
				if      (valStartup->get() == "never") language->serverStartup = Startup_Never;
				else if (valStartup->get() == "manual") language->serverStartup = Startup_Manual;
				else if (valStartup->get() == "on-file-open") language->serverStartup = Startup_OnFileOpen;
				else if (valStartup->get() == "on-app-start") language->serverStartup = Startup_OnAppStart;
				else LogWarning("%s: unknwon startup value '%.*s'", Str(nodeLanguageServer->source()), SIZE_AND_DATA(valStartup->get()));
			}
		
		skip_startup: __noop;
		}
		
		if (auto valLineComment = tblLanguage.get_as<std::string>("line-comment"))
			language->lineComment = std::move(valLineComment->get());
		
		if (auto arrBlockComment = tblLanguage.get_as<toml::array>("block-comment")) {
			for (u64 i = 0u; i < std::min(2ull, arrBlockComment->size()); i++) {
				if (toml::value<std::string>* valEnding = arrBlockComment->get_as<std::string>(i))
					language->fileEndings.push_back(std::move(valEnding->get()));
				else
					LogWarning("%s: expected a string", Str(valEnding->source()));
			}
		}
		
		if (auto tblSyntaxHighlight = tblLanguage.get_as<toml::table>("syntax-highlight")) {
			
			if (auto nodeRegex = tblSyntaxHighlight->get("regex")) {
				const bool ok = language->syntaxHighlighterRegex.FromToml(nodeRegex);
				if (ok) language->syntaxHighlighter = &language->syntaxHighlighterRegex;
			}
			
			if (auto nodeTreeSitter = tblSyntaxHighlight->get("tree-sitter")) {
				const bool ok = language->syntaxHighlighterTreeSitter.FromToml(nodeTreeSitter);
				if (ok)  language->syntaxHighlighter = &language->syntaxHighlighterTreeSitter;
			}
		}
			
		if (auto nodeDefaultSyntaxHighlighter = tblLanguage.get_as<std::string>("default-syntax-highlight")) {
			if      (nodeDefaultSyntaxHighlighter->get() == "none")  language->syntaxHighlighter = nullptr;
			else if (nodeDefaultSyntaxHighlighter->get() == "regex") language->syntaxHighlighter = &language->syntaxHighlighterRegex;
			else LogWarning("%s: unknwon default-syntax-highlight value '%.*s'", Str(nodeDefaultSyntaxHighlighter->source()), SIZE_AND_DATA(nodeDefaultSyntaxHighlighter->get()));
		}
		
		languages.push_back(std::move(language));
	}
	
	return true;
}

Language* Language::GetLanguage(std::string_view fileEnding) {
	for (auto& language : languages) {	
		for (const std::string& ending : language->fileEndings) {
			if (ending == fileEnding)
				return language;
		}
	}
	return nullptr;
}

void Language::UnloadLanguages() {
	for (Language* lang : languages)
		delete lang;
		
	languages.clear();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool Language::HasLanguageServer() const {
	return serverStartup != Startup_Never;
}

// @DUMMY
static D2D1_COLOR_F GetColorForLabel(std::string_view label) {
	if (label == "keyword")
		return D2D1::ColorF(D2D1::ColorF::RoyalBlue);
	else if (label == "function")
		return D2D1::ColorF(D2D1::ColorF::LemonChiffon);
	else if (label == "controlFlow")
		return D2D1::ColorF(D2D1::ColorF::RoyalBlue);
	else if (label == "string")
		return D2D1::ColorF(D2D1::ColorF::LightSalmon);
	else if (label == "comment")
		return D2D1::ColorF(D2D1::ColorF::LightGray);
	else if (label == "type")
		return D2D1::ColorF(D2D1::ColorF::DarkTurquoise);
	else if (label == "preprocessor")
		return D2D1::ColorF(D2D1::ColorF::HotPink);
	else if (label == "number")
		return D2D1::ColorF(D2D1::ColorF::LimeGreen);
	else
		return D2D1::ColorF(D2D1::ColorF::White);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Helper
//

static Lsp::Position ToLspPosition(TextPosition textpos) {
	return Lsp::Position {
		.line = textpos.line,
		.character = textpos.character };
}

static TextPosition ToTextPosition(Lsp::Position pos) {
	return TextPosition {
		.line = pos.line,
		.character = pos.character };
}

static Lsp::Range ToLspRange(TextPosition start, TextPosition end) {
	return Lsp::Range {
		.start = Lsp::Position {
			.line = start.line,
			.character = start.character },
		.end = Lsp::Position {
			.line = end.line,
			.character = end.character }};
}

static Lsp::TextDocumentIdentifier ToLspTextDocumentIdentifier(const Editor::TextDocumentIdentifier& textDocIdent) {
	return Lsp::TextDocumentIdentifier {
		.uri = textDocIdent.uri};
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnAutocompleteResponse(void* userdata, Lsp::CompletionResponse* response, Lsp::ErrorResponse* error) {
	auto autocomplete = static_cast<EditorAutocomplete*>(userdata);
	if (autocomplete->references == 1) {
		autocomplete->RemoveReference();
		return;
	}
	
	EditorCaretAttached::State state = EditorCaretAttached::State_Unknown;
	if (response) {
		const u64 itemCount = response->items.size();
		if (itemCount > 0u) {
		
			autocomplete->items = new EditorAutocomplete::Item[response->items.size()];
			autocomplete->itemCount = itemCount;
			
			for (u64 i = 0u; i < response->items.size(); i++) {
				EditorAutocomplete::Item& item = autocomplete->items[i];
				Lsp::CompletionResponse::Item& lspItem = response->items[i];
				
				std::string_view insertText;
				if (!lspItem.textEdit.newText.empty()) insertText = lspItem.textEdit.newText;
				else if (!lspItem.insertText.empty())  insertText = item.insertText;
				else                                   insertText = item.label;
				
				// @TODO: type
				item = EditorAutocomplete::Item {
					.type = static_cast<EditorAutocomplete::Item::Type>(lspItem.kind),
					.label = std::string {lspItem.label},
					.details = std::string {lspItem.detail},
					.documentation = std::string {lspItem.documentation.value},
					.insertPosition = ToTextPosition(lspItem.textEdit.range.start),
					.insertText = std::string {insertText}};
				
				if (lspItem.preselect)
					autocomplete->selectedItem = i;
			}
			
			if (autocomplete->selectedItem == U64_MAX)
				autocomplete->selectedItem = 0u;
				
			state = EditorCaretAttached::State_Completed;
			
			// @TODO sort items
		
		// no items
		} else {
			state = EditorCaretAttached::State_NoItems;
		}
		
	} else if (error) {
		autocomplete->error = error->message;
		state = EditorCaretAttached::State_Errored;
	
	} else {
		ASSERT_UNREACHABLE;
	}
	
	autocomplete->SortItems();
	autocomplete->state = state;
	autocomplete->RemoveReference();	
}

EditorAutocomplete* Language::GetAutoComplete(Editor* editor) {
	if (!server.IsRunning() ||
		!server.initResponse.capabilities.completionProvider.has_value()) return nullptr;
		
	auto autocomplete = EditorAutocomplete::Make(editor);
	if (!autocomplete) return nullptr;
	
	autocomplete->references = 2;
	
	server.SendCompletionRequest(
		Lsp::CompletionRequest {
			.textDocument = ToLspTextDocumentIdentifier(editor->textDocumentIdentifier),
			.position = ToLspPosition(editor->textController.carets.front().position),
			.context = Lsp::CompletionRequest::Context {
				.triggerKind = Lsp::CompletionRequest::Context::TriggerKind_Invoked,
				.triggerCharacter = {}}},
		autocomplete,
		OnAutocompleteResponse);
	
	return autocomplete;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnSignatureHelpResponse(void* userdata, Lsp::SignatureHelpResponse* response, Lsp::ErrorResponse* error) {
	auto signatureHelp = static_cast<EditorSignatureHelp*>(userdata);
	if (signatureHelp->references == 1) {
		signatureHelp->RemoveReference();
		return;
	}
	
	EditorCaretAttached::State state = EditorCaretAttached::State_Unknown;
	if (response) {
		
		if (!response->signatures.empty()) {
			u64 totalParameterCount = 0u;
			for (const Lsp::SignatureHelpResponse::Signature& sig : response->signatures)
				totalParameterCount += sig.parameters.size();
			
			signatureHelp->parameters = new EditorSignatureHelp::Parameter[totalParameterCount];
			signatureHelp->parameterCount = totalParameterCount;
			signatureHelp->signatures = new EditorSignatureHelp::Signature[response->signatures.size()];
			signatureHelp->signatureCount = response->signatures.size();
			signatureHelp->activeSignature = response->activeSignature;
			signatureHelp->activeParameter = response->activeParameter;
			
			u64 parametersUsed = 0u;
			for (u64 i = 0u; i < response->signatures.size(); i++) {
				const Lsp::SignatureHelpResponse::Signature& lspSignature = response->signatures[i];
				EditorSignatureHelp::Signature& signature = signatureHelp->signatures[i];
				
				signature.label = lspSignature.label;
				signature.documentation = lspSignature.documentation.value;
				
				const u64 parameterSpanStart = parametersUsed; 
				for (u64 j = 0u; j < lspSignature.parameters.size(); j++) {
					const Lsp::SignatureHelpResponse::Signature::Parameter& lspParameter = lspSignature.parameters[j];
					EditorSignatureHelp::Parameter& parameter = signatureHelp->parameters[parametersUsed];
					
					if (lspParameter.label.isSubstring) {
						parameter.labelIsSubstring = true;
						parameter.labelOffset   = lspParameter.label.substring.first;
						parameter.labelLength   = lspParameter.label.substring.second - parameter.labelOffset;
					} else {
						parameter.labelIsSubstring = false;
						parameter.labelData = new char[lspParameter.label.string.length()];
						memcpy(parameter.labelData, lspParameter.label.string.data(), lspParameter.label.string.length() * sizeof(char));
						parameter.labelLength = lspParameter.label.string.length();
					}
					
					parameter.documentation = lspParameter.documentation.value;
					parametersUsed++;
				}
				
				signature.parameter = std::span<EditorSignatureHelp::Parameter> {
					signatureHelp->parameters + parameterSpanStart,
					lspSignature.parameters.size() };
				signature.activeParameter = lspSignature.activeParameter;
			}
			
			ASSERT(parametersUsed == totalParameterCount);
			state = EditorCaretAttached::State_Completed;
			
		} else {
			state = EditorCaretAttached::State_NoItems;
		}
	
	} else if (error) {		
		signatureHelp->error = error->message;
		state = EditorCaretAttached::State_Errored;
	}
	
	signatureHelp->state = state;
	signatureHelp->RemoveReference();
}

EditorSignatureHelp* Language::GetSignatureHelp(Editor* editor) {
	if (!server.IsRunning() ||
		!server.initResponse.capabilities.signatureHelpProvider.has_value()) return nullptr;
	
	auto signatureHelp = EditorSignatureHelp::Make(editor);
	if (!signatureHelp) return nullptr;
	
	signatureHelp->references = 2;
	
	server.SendSignatureHelpRequest(
		Lsp::SignatureHelpRequest {
			.textDocument = Lsp::TextDocumentIdentifier {
				.uri = editor->textDocumentIdentifier.uri},
			.position = ToLspPosition(editor->textController.carets.front().position),
			.context = Lsp::SignatureHelpRequest::Context {
				.triggerKind = Lsp::SignatureHelpRequest::Context::TriggerKind_Invoked,
				.triggerCharacter = {}}},
		signatureHelp,
		OnSignatureHelpResponse);
	
	return signatureHelp;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void OnGotoResponse(void* userdata, Lsp::GotoResponse* response, Lsp::ErrorResponse* error) {
	auto textLocationList = static_cast<EditorTextLocationList*>(userdata);
	if (textLocationList->references == 1) {
		textLocationList->RemoveReference();
		return;
	}
	
	if (response) {
		if (!response->locations.empty()) {
			
			const bool extendedItems = !response->locations.front().isSimpleLocation;
			textLocationList->items = extendedItems
				? new EditorTextLocationList::ItemEx[response->locations.size()]
				: new EditorTextLocationList::Item[response->locations.size()];
			textLocationList->itemCount = response->locations.size();
			textLocationList->extendedItems = extendedItems;
		
			for (u64 i = 0u; i < response->locations.size(); i++) {
				const Lsp::LocationLink& locationLink = response->locations[i];
				EditorTextLocationList::Item& item = textLocationList->items[i];
				
				item.targetPath = MakePathFromUri(locationLink.targetUri);
				item.filename = GetFilenameFromPath(item.targetPath);
				item.selectionRange = EditorTextLocationList::Range {
					.start = ToTextPosition(locationLink.targetSelectionRange.start),
					.end =   ToTextPosition(locationLink.targetSelectionRange.end)};
				
				if (extendedItems) {
					if (locationLink.isSimpleLocation) {
						LogWarning("mixed Location and LocationLink in response to Goto*-Request");
						continue;
					}
					
					auto itemEx = static_cast<EditorTextLocationList::ItemEx&>(item);
					itemEx.originSelectionRange = EditorTextLocationList::Range {
						.start = ToTextPosition(locationLink.originSelectionRange.start),
						.end =   ToTextPosition(locationLink.originSelectionRange.end)};
					itemEx.fullTargetRange = EditorTextLocationList::Range {
						.start = ToTextPosition(locationLink.targetRange.start),
						.end =   ToTextPosition(locationLink.targetRange.end)};
				}
			}
			
			textLocationList->selectedItem = 0u;
			textLocationList->UpdateFilePreview();
			textLocationList->state = EditorCaretAttached::State_Completed;
		} else {
			textLocationList->state = EditorCaretAttached::State_NoItems;
		}
	} else if (error) {
		textLocationList->error = error->message;
		textLocationList->state = EditorCaretAttached::State_Errored;
	}
	
	textLocationList->RemoveReference();
}

template<class TGotoReq>
static EditorTextLocationList* GotoLocation(Language* self, const std::optional<Lsp::GotoServerCapabilities>& capability, Editor* editor, const TGotoReq& req) {
	if (!self->server.IsRunning()) return nullptr;
	if (!capability.has_value()) return nullptr;
	
	EditorTextLocationList* textLocationList = EditorTextLocationList::Make(editor);
	
	self->server.SendGotoRequest(req, textLocationList, OnGotoResponse);
	
	return textLocationList;
}

EditorTextLocationList* Language::GotoDecleration(Editor* editor) {
	return GotoLocation(this, 
		server.initResponse.capabilities.declarationProvider,
		editor,
		Lsp::GotoDeclerationRequest {
			.textDocument = ToLspTextDocumentIdentifier(editor->textDocumentIdentifier),
			.position = ToLspPosition(editor->textController.carets.front().position)});
}

EditorTextLocationList* Language::GotoDefinition(Editor* editor) {
	return GotoLocation(this, 
		server.initResponse.capabilities.definitionProvider,
		editor,
		Lsp::GotoDefinitionRequest {
			.textDocument = ToLspTextDocumentIdentifier(editor->textDocumentIdentifier),
			.position = ToLspPosition(editor->textController.carets.front().position)});
}

EditorTextLocationList* Language::GotoTypeDefinition(Editor* editor) {
	return GotoLocation(this, 
		server.initResponse.capabilities.typeDefinitionProvider,
		editor,
		Lsp::GotoTypeDefinitionRequest {
			.textDocument = ToLspTextDocumentIdentifier(editor->textDocumentIdentifier),
			.position = ToLspPosition(editor->textController.carets.front().position)});
}

EditorTextLocationList* Language::GotoImplementation(Editor* editor) {
	return GotoLocation(this, 
		server.initResponse.capabilities.implementationProvider,
		editor,
		Lsp::GotoImplementationRequest {
			.textDocument = ToLspTextDocumentIdentifier(editor->textDocumentIdentifier),
			.position = ToLspPosition(editor->textController.carets.front().position)});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Language::OnOpenFile(Editor* editor, std::string_view buffer) {

	if (server.state == LanguageServer::State_Standby && serverStartup == Startup_OnFileOpen) {
		server.notficationHandler = this;
		if (!server.Initialize(
				Process::StartInfo {
					.commandLine = serverStartInfo.commandLine},
				name)) {

			LogError("failed to start language server for '%s'", name.c_str());
			return;
		}
	}

	if (server.IsRunning() && server.ShouldSendOpenCloseNotification()) {
 		server.SendDidOpenNotification(Lsp::DidOpenTextDocumentNotification {
			.textDocument = Lsp::TextDocumentItem {
				.uri = editor->textDocumentIdentifier.uri,
				.languageId = name,
				.version = 0,
				.text = buffer}});
	}
	
	if (syntaxHighlighter)
		syntaxHighlighter->OnOpenFile(editor, buffer);
}

void Language::OnCloseFile(Editor* editor) {
	
	ASSERT(!editor->textDocumentIdentifier.uri.empty());
	
	if (server.IsRunning() && server.ShouldSendOpenCloseNotification()) {
		server.SendDidCloseNotification(Lsp::DidCloseTextDocumentNotification {
			.textDocument = Lsp::TextDocumentIdentifier {
				.uri = editor->textDocumentIdentifier.uri}});
	}
	
	if (syntaxHighlighter)
		syntaxHighlighter->OnCloseFile(editor);
}

void Language::OnTextBufferChanged(Editor* editor, const TextChange* change) {
	
	if (syntaxHighlighter)
		syntaxHighlighter->OnTextBufferChanged(editor, change);	
	
	if (server.IsRunning()) {

		const auto syncKind = server.GetTextDocumentSyncKind();
	
		//
		// no sync
		//
		if (syncKind == Lsp::TextDocumentSyncServerCapabilities::SyncKind_None) {
			return;
	
		//
		// incremental sync
		//	
		} else if (syncKind == Lsp::TextDocumentSyncServerCapabilities::SyncKind_Incremental) {
			ASSERT(!editor->textDocumentIdentifier.uri.empty());
			
			std::vector<Lsp::DidChangeTextDocumentNotification::ChangeEvent> changeEvents;
			changeEvents.reserve(change->count);
	
			for (usize i = 0u; i < change->count; i++) {
				const TextChangeOperation& operation = change->operations[i];
				
				if (!operation.insertedText.empty() && !operation.removedText.empty()) {
					changeEvents.push_back(Lsp::DidChangeTextDocumentNotification::ChangeEvent {
						.range = ToLspRange(operation.start, operation.removalEnd),
						.text = operation.insertedText});
				
				} else if (!operation.insertedText.empty()) {
					changeEvents.push_back(Lsp::DidChangeTextDocumentNotification::ChangeEvent {
						.range = ToLspRange(operation.start, operation.start),
						.text = operation.insertedText});
				
				} else if (!operation.removedText.empty()) {
					changeEvents.push_back(Lsp::DidChangeTextDocumentNotification::ChangeEvent {
						.range = ToLspRange(operation.start, operation.removalEnd),
						.text = {}});
				
				} else {
					ASSERT_UNREACHABLE;
				}
			}
	
			server.SendDidChangeNotification(Lsp::DidChangeTextDocumentNotification {
				.textDocument = Lsp::VersionedTextDocumentIdentifier {
					.uri = editor->textDocumentIdentifier.uri,
					.version = editor->textDocumentIdentifier.version },
				.contentChanges = changeEvents });
				//.clangdWantDiagnostics = true });
		
		//
		// full sync
		//
		} else if (syncKind == Lsp::TextDocumentSyncServerCapabilities::SyncKind_Full) {
			
			// @TODO 
			//std::string fullText;
			//editor->textBuffer.GetText(&fullText);
	
			// const auto changeEvent = LSP::DidChangeTextDocumentNotification::ChangeEvent {
			// 	.range = std::nullopt,
			// 	.text = 
	
			// }
			ASSERT_NOT_IMPLEMENTED;
			ASSERT(!editor->textDocumentIdentifier.uri.empty());
	
			server.SendDidChangeNotification(Lsp::DidChangeTextDocumentNotification {
				.textDocument = Lsp::VersionedTextDocumentIdentifier {
					.uri = editor->textDocumentIdentifier.uri,
					.version = editor->textDocumentIdentifier.version },
				.contentChanges = {},
				.clangdWantDiagnostics = false });
	
		} else {
			ASSERT_UNREACHABLE;
		}
	}
}

void Language::OnPublishDiagnostics(Lsp::PublishDiagnosticsNotification* notification) {
	
	// @TODO Use PostFunctionCall because of thread safty
	
	Editor* editor = nullptr;
	for (App::Tab& tab : app.tabs) {
		if (!tab.editor) continue;
		if (tab.editor->textDocumentIdentifier.uri == notification->uri) {
			editor = tab.editor;
			goto found_editor;
		}
	}
	
	LogWarning("diagnostics discarded: '%'.", notification->uri);
	return;

found_editor:
	const std::scoped_lock lock {editor->editorDiagnostics.mutex};
	
	editor->editorDiagnostics.records.clear();
	editor->editorDiagnostics.records.reserve(notification->diagnostics.size());
	for (const Lsp::Diagnostic& diagnostics : notification->diagnostics) {
		editor->editorDiagnostics.records.push_back(EditorDiagnostics::Record {
			.from = ToTextPosition(diagnostics.range.start),
			.to   = ToTextPosition(diagnostics.range.end),
			.code = std::string(diagnostics.code),
			.message = std::string(diagnostics.message),
			.severity = static_cast<Diagnostics::Severity>(diagnostics.severity) });
	}
	editor->editorDiagnostics.diagnosticsVersion++;	
}
