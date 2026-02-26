#pragma once
#include "language/language-server.hh"
#include "text/text-change.hh"
#include "basic-parser.hh"

#include <string_view>
#include <vector>

struct Editor;
struct ID2D1RenderTarget;
struct EditorAutocomplete;
struct EditorSignatureHelp;
struct EditorTextLocationList;

struct Language : public LanguageServer::NotificationHandler {

	//-----------------------------------------------------
	// statics

	static std::vector<Language*> languages;

	static bool LoadLanguages(std::string_view directory);
	static void UnloadLanguages();
	static Language* GetLanguage(std::string_view fileEnding);

	//-----------------------------------------------------
	// types

	struct LanguageServerStartInfo {
		
		enum Startup {
			 Startup_Never = 0,
			 Startup_Manual,
			 Startup_OnFileOpen,
			 Startup_OnAppStart
		};

		std::string_view application = {};
		std::string commandLine = {};
		Startup startup = Startup_Never;
	};

	struct SyntaxHighlighting {
		
		enum Type {
			 Type_None = 0,
			 Type_Builtin,
			 Type_TreeSitter, // @TODO not implemented
			 Type_LanguageServer
		};

		Type type = Type_None;
		std::vector<BasicParser::Rule> builtinParserRules = {};
	};
	
	//-----------------------------------------------------
	// data

	std::string name = {};
	std::vector<std::string> fileEndings = {};

	LanguageServerStartInfo serverStartInfo = {};
	LanguageServer          server = {};

	SyntaxHighlighting syntaxHighlighting = {};
	
	std::string lineComment = {};
	std::string blockComment[2] = {};

	//-----------------------------------------------------
	// functions
	
	bool HasLanguageServer() const;

	void HighlightSyntax(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine);
	EditorAutocomplete* GetAutoComplete(Editor* editor);
	EditorSignatureHelp* GetSignatureHelp(Editor* editor);
	
	EditorTextLocationList* GotoDecleration(Editor* editor);
	EditorTextLocationList* GotoDefinition(Editor* editor);
	EditorTextLocationList* GotoTypeDefinition(Editor* editor);
	EditorTextLocationList* GotoImplementation(Editor* editor);
	
	void OnOpenFile(Editor* editor, std::string_view buffer);
	void OnCloseFile(Editor* editor);
	void OnTextBufferChanged(Editor* editor, const TextChange* change);
	
private:
	virtual void OnPublishDiagnostics(Lsp::PublishDiagnosticsNotification* notification) override;
};