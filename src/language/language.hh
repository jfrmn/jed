#pragma once
#include "language/language-server.hh"
#include "language/syntaxhighlighter-regex.hh"
#include "language/syntaxhighlighter-treesitter.hh"
#include "text/text-change.hh"
#include "util/color.hh"
#include "util/hashtable.hh"

#include <string_view>
#include <vector>

struct ID2D1RenderTarget;
struct Tool;
struct Editor;
struct EditorAutocomplete;
struct EditorSignatureHelp;
struct EditorTextLocationList;

struct Language : public LanguageServer::NotificationHandler {

	//-----------------------------------------------------
	// statics
	//-----------------------------------------------------

	static std::vector<Language*> languages;

	static bool LoadLanguages(std::string_view directory);
	static bool LoadLanguages2(std::string_view directory);
	static void UnloadLanguages();
	static Language* GetLanguage(std::string_view fileEnding);

	//-----------------------------------------------------
	// types
	//-----------------------------------------------------

	enum Startup {
		 Startup_Never = 0,
		 Startup_Manual,
		 Startup_OnFileOpen,
		 Startup_OnAppStart
	};
	
	// @TODO use ProcessStartInfo
	struct LanguageServerStartInfo {
		std::string commandLine = {};
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	std::string name = {};
	std::vector<std::string> fileEndings = {};

	LanguageServerStartInfo serverStartInfo = {};
	Startup                 serverStartup = Startup_Never;
	LanguageServer          server = {};
	
	SyntaxHighlighter*          syntaxHighlighter = nullptr;
	SyntaxHighlighterRegex      syntaxHighlighterRegex = {};
	SyntaxHighlighterTreeSitter syntaxHighlighterTreeSitter = {};
	Hashtable<Color>            syntaxColors = {};
	
	std::vector<Tool> tools = {};
		
	std::string lineComment = {};
	std::string blockComment[2] = {};

	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	bool HasLanguageServer() const;

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