#pragma once
#include "logging.hh"
#include "language/language-server-protocol.hh"
#include "language/json-helper.hh"
#include "util/process.hh"


#include <atomic>
#include <unordered_map>

struct TextEdit;
struct TextBuffer;

struct LanguageServer : public Process::Observer {
	
	//-----------------------------------------------------
	// types

	struct NotificationHandler {
		virtual void OnPublishDiagnostics(Lsp::PublishDiagnosticsNotification* notification) = 0;
	};

	template<class TResp>
	using FuncOnResponse = void (*) (void* userdata, TResp* response, Lsp::ErrorResponse* error);
	
	enum State {
		State_Standby = 0,
		State_Initializing,
		State_Running,
		State_ShuttingDown,
		State_Exited,
		State_Crashed
	};

	struct PendingRequest {
		
		void* userdata = nullptr;
		void (*OnResponse) (void* userdata, void* response, Lsp::ErrorResponse* error) = nullptr;

		std::unique_ptr<Lsp::Response> responseData = nullptr;
		void (*ReadResponseJson) (const cJSON*, void*) = nullptr;
	};

	//-----------------------------------------------------
	// data

	Logger logger = {};

	std::atomic<State> state = State_Standby;
	Process process = {};
	Lsp::InitializeResponse initResponse = {};

	JsonAllocator jsonAllocator;

	usize expectedSize = 0u;
	std::string readBuffer = {};
	
	u64 requestIdCounter = 0u;
	std::unordered_map<u64, PendingRequest> pendingRequests = {};

	NotificationHandler* notficationHandler = nullptr;

	//-----------------------------------------------------
	// functions

	bool Initialize(Process::StartInfo startInfo, std::string_view languageName = {});
	bool Exit();

	bool IsRunning() const { return state == State_Running; }
	bool ShouldSendOpenCloseNotification() const;
	Lsp::TextDocumentSyncServerCapabilities::SyncKind GetTextDocumentSyncKind() const;

	bool SendDidOpenNotification(const Lsp::DidOpenTextDocumentNotification& notification);
	bool SendDidCloseNotification(const Lsp::DidCloseTextDocumentNotification& notification);
	bool SendDidChangeNotification(const Lsp::DidChangeTextDocumentNotification& notification);

	bool SendCompletionRequest(const Lsp::CompletionRequest& request, void* ud, FuncOnResponse<Lsp::CompletionResponse> funcOnResponse);
	bool SendSignatureHelpRequest(const Lsp::SignatureHelpRequest& request, void* ud, FuncOnResponse<Lsp::SignatureHelpResponse> funcOnResponse);
	
	bool SendGotoRequest(const Lsp::GotoDeclerationRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse);
	bool SendGotoRequest(const Lsp::GotoDefinitionRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse);
	bool SendGotoRequest(const Lsp::GotoTypeDefinitionRequest& rueqest, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnReospnse);
	bool SendGotoRequest(const Lsp::GotoImplementationRequest& rueqest, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnReospnse);

private:
	virtual void OnStderr(std::string_view data) override;
	virtual void OnStdout(std::string_view data) override;

	virtual void OnStarted() override;
	virtual void OnExited(int exitCode) override;
};