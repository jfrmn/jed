#include "language-server.hh"
#include "util/logging.hh"

#include <cJSON/cJSON.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define STATIC_PRINT_BUFFER_SIZE 1024
#define TIMEOUT_INIT_AND_SHUTDOWN_REQUESTS 5000

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template<class TReq, class TResp>
static bool SendRequest(LanguageServer* self, const TReq& request, void* userdata, LanguageServer::FuncOnResponse<TResp> funcOnResp) {
	
	if constexpr (!std::is_same<TReq, Lsp::InitializeRequest>::value) {
		if (self->state != LanguageServer::State_Running)
			return false;
	}
	
	Logger* prevLogger = SetActiveLogger(&self->logger);
	DEFER(activeLogger = prevLogger);
	
	const u64 requestId = self->requestIdCounter++;
	LogInfo("sending request % '%'", requestId, TReq::METHOD);

	//
	// place pending request
	//
	{	
		void (*ReadResponseJson) (const cJSON*, TResp*) = ::ReadJson;

		LanguageServer::PendingRequest pendingRequest;
		pendingRequest.userdata = userdata,
		pendingRequest.OnResponse = reinterpret_cast<void(*)(void*, void*, Lsp::ErrorResponse*)>(funcOnResp),
		pendingRequest.ReadResponseJson = reinterpret_cast<void(*)(const cJSON*, void*)>(ReadResponseJson),
		pendingRequest.responseData.reset(new TResp());

		const auto pairResult = self->pendingRequests.emplace(requestId, std::move(pendingRequest));
		ASSERT(pairResult.second);
	}

	 char staticBuffer[STATIC_PRINT_BUFFER_SIZE] {0};	
	JsonWriteBuffer writeBuffer {staticBuffer};
	
	//
	// create json
	//
	{
		writeBuffer.WriteObjectStart();
		writeBuffer.WriteProperty(Lsp::JSONRPC_VERSION)->WriteRawString("2.0");
		writeBuffer.WriteProperty(Lsp::JSONRPC_ID)->WriteUnsigned(requestId);
		writeBuffer.WriteProperty(Lsp::JSONRPC_METHOD)->WriteRawString(TReq::METHOD);
		writeBuffer.WriteProperty(Lsp::JSONRPC_PARAMS);
		WriteJson(&request, &writeBuffer);
		writeBuffer.WriteObjectEnd();
	}
	
	//
	// send message
	//	
	{
		const std::string_view contentString = writeBuffer.GetString();
		activeLogger->LogWithArgs(LogLevel_Detail, contentString, {});

		char headerBuffer[32] {};
		const u64 headerSize = FormatToBuffer(headerBuffer, "Content-Length: %\r\n\r\n", contentString.length());
	
		bool res = true;
		res &= self->process.WriteToStdin(std::string_view {headerBuffer, headerSize});
		res &= self->process.WriteToStdin(contentString);
	
		if (!res) {
			LogError("writing message failed");
			self->pendingRequests.erase(requestId);
			return false;
		}
	}
	
	return true;
}

template<class TNotif>
static bool SendNotification(LanguageServer* self, const TNotif& notification) {

	if constexpr (!std::is_same<TNotif, Lsp::InitializedNotification>::value) {
		if (self->state != LanguageServer::State_Running)
			return false;
	}
	
	LogInfo("sending notification '%'", TNotif::METHOD);
			
	Logger* prevLogger = SetActiveLogger(&self->logger);
	DEFER(activeLogger = prevLogger);

	char staticBuffer[STATIC_PRINT_BUFFER_SIZE] {0};	
	JsonWriteBuffer writeBuffer {staticBuffer};
	
	//
	// create json
	//
	{
		writeBuffer.WriteObjectStart();
		writeBuffer.WriteProperty(Lsp::JSONRPC_VERSION)->WriteRawString("2.0");
		writeBuffer.WriteProperty(Lsp::JSONRPC_METHOD)->WriteRawString(TNotif::METHOD);
		writeBuffer.WriteProperty(Lsp::JSONRPC_PARAMS);
		WriteJson(&notification, &writeBuffer);
		writeBuffer.WriteObjectEnd();
	}
	
	//
	// send message
	//	
	{
		const std::string_view contentString = writeBuffer.GetString();
		activeLogger->LogWithArgs(LogLevel_Detail, contentString, {});

		char headerBuffer[32] {};
		const u64 headerSize = FormatToBuffer(headerBuffer, "Content-Length: %\r\n\r\n", contentString.length());
	
		bool res = true;
		res &= self->process.WriteToStdin(std::string_view {headerBuffer, headerSize});
		res &= self->process.WriteToStdin(contentString);
	
		if (!res) {
			LogError("writing message failed");
			return false;
		}
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static bool ParseHeader(LanguageServer* self, std::string_view data, /*out*/ std::string_view& content, /*out*/ Lsp::Header& header) {

	while (!data.empty()) {

		if (data.starts_with(Lsp::HEADER_CONTENT_LENGTH)) {
			data.remove_prefix(Lsp::HEADER_CONTENT_LENGTH.length());

			const std::from_chars_result fromCharsResult = std::from_chars(data.data(), data.data() + data.length(), header.contentLength);

			if (fromCharsResult.ec != std::errc()) {
				LogError("failed to parse content-length: %", data.substr(0, 100));
				return false;
			}

			const auto offset = (fromCharsResult.ptr - data.data());
			data = data.substr(offset + Lsp::HEADER_DELIMITER.length());
		
		} else if (data.starts_with(Lsp::HEADER_CONTENT_TYPE)) {
			data.remove_prefix(Lsp::HEADER_CONTENT_TYPE.length());

			auto offsetToDelim = data.find(Lsp::HEADER_DELIMITER);
			header.contentType = data.substr(0, offsetToDelim);
			data = data.substr(offsetToDelim + Lsp::HEADER_DELIMITER.length());
		
		} else if (data.starts_with(Lsp::HEADER_DELIMITER)) {
			data.remove_prefix(Lsp::HEADER_DELIMITER.length());
			content = data;
			return true;

		} else {
			ASSERT_UNREACHABLE;
			return false;
		}
	}

	ASSERT_UNREACHABLE;
	return false;
}

static bool ProcessMessage(LanguageServer* self) {
	
	//
	// parse message
	//
	const char* errpos = nullptr;
	cJSON* json = cJSON_ParseWithLengthOpts(self->readBuffer.data(), self->readBuffer.size(), &errpos, false);
	if (!json) {
		LogError("failed to parse message (error at pos %):\n%", errpos - self->readBuffer.data(), self->readBuffer);
		return false;
	}
	JsonObjectReader objectReader {json};
	
	//
	// response?
	//
	if (u64 requestId = 0u; objectReader.ReadUnsigned(Lsp::JSONRPC_ID, &requestId)) {
		LogInfo("recieved response to %", requestId);
		
		LanguageServer::PendingRequest pendingRequest;
				
		// extract response
		if (auto node = self->pendingRequests.extract(requestId)) {
			pendingRequest = std::move(node.mapped());
		
		} else {
			LogError("no pending request for id %", requestId);
			return false;
		}

		//
		// successfull response?
		//
		if (const cJSON* jsonResult = objectReader.Get(Lsp::JSONRPC_RESULT)) {

			pendingRequest.ReadResponseJson(jsonResult, pendingRequest.responseData.get());
			pendingRequest.OnResponse(pendingRequest.userdata, pendingRequest.responseData.get(), nullptr);
			return true;

		//
		// error response?
		//
		} else if (const cJSON* jsonError = objectReader.Get(Lsp::JSONRPC_ERROR)) {

			Lsp::ErrorResponse error {};
			ReadJson(jsonError, &error);
			pendingRequest.OnResponse(pendingRequest.userdata, nullptr, &error);
			return true;
		
		} else {
			Lsp::ErrorResponse error {
				.code = Lsp::ErrorResponse::Code_ClientParseError,
				.message = "failed to parse reponse data"};
			pendingRequest.OnResponse(pendingRequest.userdata, nullptr, &error);
		}

		return false;

	//
	// notification?
	//
	} else if (std::string_view method; objectReader.ReadString(Lsp::JSONRPC_METHOD, &method)) {

		LogInfo("recieved notification '%'", method);
		
		const cJSON* jsonParams = objectReader.Get(Lsp::JSONRPC_PARAMS);
		if (!jsonParams) {
			LogError("notification is missing property 'params'");
			return false;
		}
		
		ASSERT(self->notficationHandler);
		
		if (method == Lsp::PublishDiagnosticsNotification::METHOD) {
			
			Lsp::PublishDiagnosticsNotification diagnosticsNotification;
			ReadJson(jsonParams, &diagnosticsNotification);
			self->notficationHandler->OnPublishDiagnostics(&diagnosticsNotification);
			return true;
		
		} else {
			LogWarning("notification not supported");
		}

	// something went wrong
	} else {
		LogError("recieved json object has neither 'id' nor 'method'-property");
		return false;
	}

	return true;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct EventUserdata {
	LanguageServer* server = nullptr;
	HANDLE hEvent = NULL;
};

static void OnInitResponse(void* userdata, Lsp::InitializeResponse* response, Lsp::ErrorResponse* errorResponse) {
	auto self = static_cast<EventUserdata*>(userdata)->server;
	auto hEvent = static_cast<EventUserdata*>(userdata)->hEvent;

	if (response) {
		LogDetail("recieved initialize response");
		self->initResponse = std::move(*response);
	
	} else {
		LogError("initialize request failed: %", errorResponse);
	}

	SetEvent(hEvent);
}

bool LanguageServer::Initialize(Process::StartInfo startInfo, std::string_view languageName /*= {}*/) {

	Logger* prevLogger = SetActiveLogger(&logger);
	DEFER(activeLogger = prevLogger);

	//
	// init logger
	//	
	{
		logger = Logger {
			.level  = prevLogger->level,
			.prefix = languageName,
			.out    = prevLogger->out,
			.mtx    = prevLogger->mtx };
	}

	//
	// start process
	//
	{
		process.observer = this;

		if (!process.Start(std::move(startInfo))) {
			LogError("init process failed.");
			return false;
		}
	}

	LogInfo("server initializing");
	state = State_Initializing;

	//
	// send init request
	//
	{
		Lsp::InitializeRequest initRequest {};
		GetProcessId(&initRequest.processId);
		initRequest.clientInfo.name = "slick-edit";
		initRequest.rootPath = "D:\\Projects\\slick-edit-3"; // @DUMMY
		initRequest.capabilities.general.positionEncodings = {Lsp::PositionEncodingKind_Utf8};
		initRequest.clangdOffsetEncoding.emplace({Lsp::PositionEncodingKind_Utf8});

		HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (!hEvent) {
			LogError("CreateEvent() failed");
			return false;
		}
		
		DEFER(CloseHandle(hEvent));

		EventUserdata userdata {this, hEvent};

		if (!SendRequest(this, initRequest, &userdata, OnInitResponse)) {
			LogError("sending init-request failed");
			return false;
		}

		const auto res = WaitForSingleObject(hEvent, TIMEOUT_INIT_AND_SHUTDOWN_REQUESTS);

		if (res != WAIT_OBJECT_0) {
			LogWarning("waiting for initialize response failed: %", FWaitRes(res));
			return false;
		}
	}

	//
	// send init notification
	//
	{
		Lsp::InitializedNotification initNotification {};
		if (!SendNotification(this, initNotification)) {
			LogError("sending initiallaized notification failed");
			return false;
		}
	}

	LogInfo("server initalized");
	state = State_Running;
	return true;
}

static void OnShutdownResponse(void *ud, Lsp::ShutdownResponse *response, Lsp::ErrorResponse *error) {
	auto self = static_cast<EventUserdata*>(ud)->server;
	auto hEvent = static_cast<EventUserdata*>(ud)->hEvent;

	if (response) {
		LogInfo("server shut down");
	} else {
		//LogWarning("server shutdown failed: $", error);
	}

	SetEvent(hEvent);
}

bool LanguageServer::Exit() {

	LogInfo("shutting down...");
	state = State_ShuttingDown;

	//
	// send shutdown request
	//
	{
		HANDLE hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
		if (!hEvent) {
			LogError("CreateEvent() failed");
			return false;
		}
		
		DEFER(CloseHandle(hEvent));
		
		EventUserdata userdata {this, hEvent};

		Lsp::ShutdownRequest shutdownRequest {};
		if (!SendRequest(this, shutdownRequest, &userdata, OnShutdownResponse)) {
			LogError("sending shutdown request failed");
			return false;
		}

		const auto res = WaitForSingleObject(hEvent, TIMEOUT_INIT_AND_SHUTDOWN_REQUESTS);
		CloseHandle(hEvent);

		if (res != WAIT_OBJECT_0) {
			LogWarning("waiting for shutdown response failed: %", FWaitRes(res));
			return false;
		}
	}

	// make sure we set the state befor we sent the notifcation
	// so that we don't accidentlly override the status on OnExit()
	state = State_Exited;
	
	//
	// send exit notification 
	//
	{
		Lsp::ExitNotification exitNotification;
		if (!SendNotification(this, exitNotification)) {
			LogError("sending exit-notification failed");
			return false;
		}
	}

	LogInfo("server exitied");
	return true;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LanguageServer::ShouldSendOpenCloseNotification() const {
	return initResponse.capabilities.textDocumentSync.has_value()
		&& initResponse.capabilities.textDocumentSync.value().openClose;
}

Lsp::TextDocumentSyncServerCapabilities::SyncKind LanguageServer::GetTextDocumentSyncKind() const {
	if (!initResponse.capabilities.textDocumentSync.has_value())
		return Lsp::TextDocumentSyncServerCapabilities::SyncKind_None;

	return static_cast<Lsp::TextDocumentSyncServerCapabilities::SyncKind>(
		initResponse.capabilities.textDocumentSync.value().change);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LanguageServer::SendDidOpenNotification(const Lsp::DidOpenTextDocumentNotification& notification) {
	return SendNotification(this, notification);
}

bool LanguageServer::SendDidCloseNotification(const Lsp::DidCloseTextDocumentNotification& notification) {
	return SendNotification(this, notification);
}

bool LanguageServer::SendDidChangeNotification(const Lsp::DidChangeTextDocumentNotification& notification) {
	return SendNotification(this, notification);
}

bool LanguageServer::SendCompletionRequest(const Lsp::CompletionRequest& request, void* ud, FuncOnResponse<Lsp::CompletionResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

bool LanguageServer::SendSignatureHelpRequest(const Lsp::SignatureHelpRequest& request, void* ud, FuncOnResponse<Lsp::SignatureHelpResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

bool LanguageServer::SendGotoRequest(const Lsp::GotoDeclerationRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

bool LanguageServer::SendGotoRequest(const Lsp::GotoDefinitionRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

bool LanguageServer::SendGotoRequest(const Lsp::GotoTypeDefinitionRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

bool LanguageServer::SendGotoRequest(const Lsp::GotoImplementationRequest& request, void* ud, FuncOnResponse<Lsp::GotoResponse> funcOnResponse) {
	return SendRequest(this, request, ud, funcOnResponse);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void LanguageServer::OnStderr(std::string_view data) {
	// @FIXME check if data actually ends with a linebreak
	data.remove_suffix(2); // remove linebreak
	LogInfo("[log] %", data);
}

void LanguageServer::OnStdout(std::string_view data) {

	if (expectedSize == 0u) {

		Lsp::Header header {};
		std::string_view content {};
		
		if (!ParseHeader(this, data, content, header)) {
			LogError("failed to parse header (% bytes):\n%", data.length(), data.substr(0, 100));
			return;
		}
		
		expectedSize = header.contentLength;
		readBuffer.reserve(header.contentLength);
		readBuffer.append(content);
	
	} else {
		readBuffer.append(data);
	}

	//ASSERT(readBuffer.size() <= expectedSize);
	if (readBuffer.size() == expectedSize) {
		
		LogDetail("message recieved (length: %)\n%", readBuffer.size(), readBuffer);
		ProcessMessage(this);
		
		expectedSize = 0u;
		readBuffer.clear();
		jsonAllocator.Reset();
	}
}

void LanguageServer::OnStarted() {
	activeLogger = &logger;
	JsonAllocator::activeAllocator = &jsonAllocator;
}

void LanguageServer::OnExited(int exitCode) {
		
	if (state != State_Exited)
		state = State_Crashed;
	
	LogWarning("server exited unexpectedly (code: %)", exitCode);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//LanguageServer::PendingRequest::PendingRequest(PendingRequest &&other)
	//: userdata(other.userdata)
	//, funcOnResponse(other.funcOnResponse)
	//, funcResponseFromJson(other.funcResponseFromJson)
	//, responseData(other.responseData) {
	//other.responseData = nullptr;
//}

//LanguageServer::PendingRequest::~PendingRequest() {
	//delete responseData;
//}

//LanguageServer::PendingRequest &LanguageServer::PendingRequest::operator=(PendingRequest &&other) {
	//userdata = other.userdata;
	//funcOnResponse = other.funcOnResponse;
	//funcResponseFromJson = other.funcResponseFromJson;
	//std::swap(responseData, other.responseData);
	//return *this;
//}
