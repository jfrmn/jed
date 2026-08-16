#include "doctest/doctest.h"
#include "language/language-server-protocol.hh"
#include "language/json-helper.hh"
#include <cJSON/cJSON.h>
#include <string>
#include <ostream> // won't compile without this - idk why

namespace Lsp = LanguageServerProtocol;

TEST_SUITE("ReadJson and WriteJson Functions") {

    //=========================================================================
    // Helper Functions
    //=========================================================================

    // Helper to create a JSON buffer for testing
    std::string WriteToBuffer(auto value) {
        char buffer[4096] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(value, &writeBuffer);
        return std::string(writeBuffer.GetString());
    }

    //=========================================================================
    // Position Tests
    //=========================================================================

    TEST_CASE("Position - ReadJson with valid object") {
        const char* jsonStr = R"({"line": 10, "character": 5})";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::Position pos;
        ReadJson(json, &pos);
        
        CHECK(pos.line == 10);
        CHECK(pos.character == 5);
        
        cJSON_Delete(json);
    }

    TEST_CASE("Position - ReadJson with zero values") {
        const char* jsonStr = R"({"line": 0, "character": 0})";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::Position pos;
        ReadJson(json, &pos);
        
        CHECK(pos.line == 0);
        CHECK(pos.character == 0);
        
        cJSON_Delete(json);
    }

    TEST_CASE("Position - WriteJson") {
        Lsp::Position pos;
        pos.line = 42;
        pos.character = 15;
        
        char buffer[512] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&pos, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"line\"") != std::string::npos);
        CHECK(result.find("\"character\"") != std::string::npos);
        CHECK(result.find("42") != std::string::npos);
        CHECK(result.find("15") != std::string::npos);
    }

    //=========================================================================
    // Range Tests
    //=========================================================================

    TEST_CASE("Range - ReadJson with valid object") {
        const char* jsonStr = R"({
            "start": {"line": 1, "character": 2},
            "end": {"line": 3, "character": 4}
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::Range range;
        ReadJson(json, &range);
        
        CHECK(range.start.line == 1);
        CHECK(range.start.character == 2);
        CHECK(range.end.line == 3);
        CHECK(range.end.character == 4);
        
        cJSON_Delete(json);
    }

    TEST_CASE("Range - WriteJson") {
        Lsp::Range range;
        range.start.line = 10;
        range.start.character = 5;
        range.end.line = 20;
        range.end.character = 15;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&range, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"start\"") != std::string::npos);
        CHECK(result.find("\"end\"") != std::string::npos);
    }

    //=========================================================================
    // Location Tests
    //=========================================================================

    TEST_CASE("Location - ReadJson with valid object") {
        const char* jsonStr = R"({
            "uri": "file:///test/file.txt",
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 1, "character": 1}
            }
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::Location loc;
        ReadJson(json, &loc);
        
        CHECK(loc.uri == "file:///test/file.txt");
        CHECK(loc.range.start.line == 0);
        CHECK(loc.range.start.character == 0);
        
        cJSON_Delete(json);
    }

    TEST_CASE("Location - WriteJson") {
        Lsp::Location loc;
        loc.uri = "file:///test/location.txt";
        loc.range.start.line = 5;
        loc.range.start.character = 10;
        loc.range.end.line = 5;
        loc.range.end.character = 20;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&loc, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"uri\"") != std::string::npos);
        CHECK(result.find("\"range\"") != std::string::npos);
    }

    //=========================================================================
    // TextDocumentIdentifier Tests
    //=========================================================================

    TEST_CASE("TextDocumentIdentifier - ReadJson with valid object") {
        const char* jsonStr = R"({"uri": "file:///document.txt"})";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TextDocumentIdentifier doc;
        ReadJson(json, &doc);
        
        CHECK(doc.uri == "file:///document.txt");
        
        cJSON_Delete(json);
    }

    TEST_CASE("TextDocumentIdentifier - WriteJson") {
        Lsp::TextDocumentIdentifier doc;
        doc.uri = "file:///my/document.cpp";
        
        char buffer[512] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&doc, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"uri\"") != std::string::npos);
    }

    //=========================================================================
    // VersionedTextDocumentIdentifier Tests
    //=========================================================================

    TEST_CASE("VersionedTextDocumentIdentifier - ReadJson with valid object") {
        const char* jsonStr = R"({"uri": "file:///doc.txt", "version": 42})";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::VersionedTextDocumentIdentifier doc;
        ReadJson(json, &doc);
        
        CHECK(doc.uri == "file:///doc.txt");
        CHECK(doc.version == 42);
        
        cJSON_Delete(json);
    }

    TEST_CASE("VersionedTextDocumentIdentifier - WriteJson") {
        Lsp::VersionedTextDocumentIdentifier doc;
        doc.uri = "file:///versioned.txt";
        doc.version = 100;
        
        char buffer[512] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&doc, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"uri\"") != std::string::npos);
        CHECK(result.find("\"version\"") != std::string::npos);
    }

    //=========================================================================
    // MarkupKind Tests
    //=========================================================================

    TEST_CASE("MarkupKind - ReadJson plaintext") {
        const char* jsonStr = R"("plaintext")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::MarkupKind kind;
        ReadJson(json, &kind);
        
        CHECK(kind == Lsp::MarkupKind_Plaintext);
        
        cJSON_Delete(json);
    }

    TEST_CASE("MarkupKind - ReadJson markdown") {
        const char* jsonStr = R"("markdown")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::MarkupKind kind;
        ReadJson(json, &kind);
        
        CHECK(kind == Lsp::MarkupKind_Markdown);
        
        cJSON_Delete(json);
    }

    TEST_CASE("MarkupKind - WriteJson plaintext") {
        Lsp::MarkupKind kind = Lsp::MarkupKind_Plaintext;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&kind, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("plaintext") != std::string::npos);
    }

    TEST_CASE("MarkupKind - WriteJson markdown") {
        Lsp::MarkupKind kind = Lsp::MarkupKind_Markdown;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&kind, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("markdown") != std::string::npos);
    }

    //=========================================================================
    // MarkupString Tests
    //=========================================================================

    TEST_CASE("MarkupString - ReadJson with plaintext") {
        const char* jsonStr = R"("This is plain text")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::MarkupString markup;
        ReadJson(json, &markup);
        
        CHECK(markup.kind == Lsp::MarkupKind_Plaintext);
        CHECK(markup.value == "This is plain text");
        
        cJSON_Delete(json);
    }

    TEST_CASE("MarkupString - ReadJson with object") {
        const char* jsonStr = R"({
            "kind": "markdown",
            "value": "# Title"
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::MarkupString markup;
        ReadJson(json, &markup);
        
        CHECK(markup.kind == Lsp::MarkupKind_Markdown);
        CHECK(markup.value == "# Title");
        
        cJSON_Delete(json);
    }

    TEST_CASE("MarkupString - WriteJson") {
        Lsp::MarkupString markup;
        markup.kind = Lsp::MarkupKind_Markdown;
        markup.value = "**Bold text**";
        
        char buffer[512] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&markup, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("markdown") != std::string::npos);
        CHECK(result.find("**Bold text**") != std::string::npos);
    }

    //=========================================================================
    // TextEdit Tests
    //=========================================================================

    TEST_CASE("TextEdit - ReadJson with valid object") {
        const char* jsonStr = R"({
            "range": {
                "start": {"line": 0, "character": 0},
                "end": {"line": 0, "character": 4}
            },
            "newText": "replacement"
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TextEdit edit;
        ReadJson(json, &edit);
        
        CHECK(edit.range.start.line == 0);
        CHECK(edit.range.start.character == 0);
        CHECK(edit.newText == "replacement");
        
        cJSON_Delete(json);
    }

    TEST_CASE("TextEdit - WriteJson") {
        Lsp::TextEdit edit;
        edit.range.start.line = 1;
        edit.range.start.character = 5;
        edit.range.end.line = 1;
        edit.range.end.character = 10;
        edit.newText = "updated";
        
        char buffer[512] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&edit, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("\"range\"") != std::string::npos);
        CHECK(result.find("\"newText\"") != std::string::npos);
    }

    //=========================================================================
    // PositionEncodingKind Tests
    //=========================================================================

    TEST_CASE("PositionEncodingKind - ReadJson utf-8") {
        const char* jsonStr = R"("utf-8")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::PositionEncodingKind kind;
        ReadJson(json, &kind);
        
        CHECK(kind == Lsp::PositionEncodingKind_Utf8);
        
        cJSON_Delete(json);
    }

    TEST_CASE("PositionEncodingKind - ReadJson utf-16") {
        const char* jsonStr = R"("utf-16")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::PositionEncodingKind kind;
        ReadJson(json, &kind);
        
        CHECK(kind == Lsp::PositionEncodingKind_Utf16);
        
        cJSON_Delete(json);
    }

    TEST_CASE("PositionEncodingKind - ReadJson utf-32") {
        const char* jsonStr = R"("utf-32")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::PositionEncodingKind kind;
        ReadJson(json, &kind);
        
        CHECK(kind == Lsp::PositionEncodingKind_Utf32);
        
        cJSON_Delete(json);
    }

    TEST_CASE("PositionEncodingKind - WriteJson utf-8") {
        Lsp::PositionEncodingKind kind = Lsp::PositionEncodingKind_Utf8;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&kind, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("utf-8") != std::string::npos);
    }

    //=========================================================================
    // TraceValue Tests
    //=========================================================================

    TEST_CASE("TraceValue - ReadJson off") {
        const char* jsonStr = R"("off")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TraceValue value;
        ReadJson(json, &value);
        
        CHECK(value == Lsp::TraceValue_Off);
        
        cJSON_Delete(json);
    }

    TEST_CASE("TraceValue - ReadJson messages") {
        const char* jsonStr = R"("messages")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TraceValue value;
        ReadJson(json, &value);
        
        CHECK(value == Lsp::TraceValue_Messages);
        
        cJSON_Delete(json);
    }

    TEST_CASE("TraceValue - ReadJson verbose") {
        const char* jsonStr = R"("verbose")";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TraceValue value;
        ReadJson(json, &value);
        
        CHECK(value == Lsp::TraceValue_Verbose);
        
        cJSON_Delete(json);
    }

    TEST_CASE("TraceValue - WriteJson") {
        Lsp::TraceValue value = Lsp::TraceValue_Verbose;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&value, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("verbose") != std::string::npos);
    }

    //=========================================================================
    // GotoServerCapabilities Tests
    //=========================================================================

    TEST_CASE("GotoServerCapabilities - ReadJson with valid object") {
        const char* jsonStr = R"({"workDoneProgress": true})";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::GotoServerCapabilities caps;
        ReadJson(json, &caps);
        
        CHECK(caps.workDoneProgress == true);
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // GotoResponse Tests
    //=========================================================================

    TEST_CASE("GotoResponse - ReadJson with empty locations") {
        const char* jsonStr = R"([])";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::GotoResponse response;
        ReadJson(json, &response);
        
        CHECK(response.locations.empty());
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // ErrorResponse Tests
    //=========================================================================

    TEST_CASE("ErrorResponse - ReadJson with valid object") {
        const char* jsonStr = R"({
            "code": -32700,
            "message": "Parse error"
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::ErrorResponse error;
        ReadJson(json, &error);
        
        CHECK(error.code == -32700);
        CHECK(error.message == "Parse error");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // MessageNotification Tests
    //=========================================================================

    TEST_CASE("MessageNotification - ReadJson with valid object") {
        const char* jsonStr = R"({
            "type": 1,
            "message": "Test message"
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::MessageNotification notif;
        ReadJson(json, &notif);
        
        CHECK(notif.message == "Test message");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // Diagnostic Tests
    //=========================================================================

    TEST_CASE("Diagnostic - ReadJson with valid object") {
        const char* jsonStr = R"({
            "range": {
                "start": {"line": 5, "character": 10},
                "end": {"line": 5, "character": 20}
            },
            "message": "Error message",
            "severity": 1
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::Diagnostic diag;
        ReadJson(json, &diag);
        
        CHECK(diag.range.start.line == 5);
        CHECK(diag.message == "Error message");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // GotoClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("GotoClientCapabilities - WriteJson") {
        Lsp::GotoClientCapabilities caps;
        caps.linkSupport = true;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("linkSupport") != std::string::npos);
        CHECK(result.find("true") != std::string::npos);
    }

    //=========================================================================
    // GotoDeclerationRequest WriteJson Test
    //=========================================================================

    TEST_CASE("GotoDeclerationRequest - WriteJson") {
        Lsp::GotoDeclerationRequest req;
        req.textDocument.uri = "file:///test.txt";
        req.position.line = 10;
        req.position.character = 5;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("textDocument") != std::string::npos);
        CHECK(result.find("position") != std::string::npos);
    }

    //=========================================================================
    // GotoDefinitionRequest WriteJson Test
    //=========================================================================

    TEST_CASE("GotoDefinitionRequest - WriteJson") {
        Lsp::GotoDefinitionRequest req;
        req.textDocument.uri = "file:///source.cpp";
        req.position.line = 0;
        req.position.character = 0;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // GotoTypeDefinitionRequest WriteJson Test
    //=========================================================================

    TEST_CASE("GotoTypeDefinitionRequest - WriteJson") {
        Lsp::GotoTypeDefinitionRequest req;
        req.textDocument.uri = "file:///types.cpp";
        req.position.line = 1;
        req.position.character = 1;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // GotoImplementationRequest WriteJson Test
    //=========================================================================

    TEST_CASE("GotoImplementationRequest - WriteJson") {
        Lsp::GotoImplementationRequest req;
        req.textDocument.uri = "file:///impl.cpp";
        req.position.line = 2;
        req.position.character = 2;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // GotoReferencesRequest WriteJson Test
    //=========================================================================

    TEST_CASE("GotoReferencesRequest - WriteJson") {
        Lsp::GotoReferencesRequest req;
        req.textDocument.uri = "file:///refs.cpp";
        req.position.line = 3;
        req.position.character = 3;
        req.context.includeDecleration = true;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // CompletionRequest WriteJson Test
    //=========================================================================

    TEST_CASE("CompletionRequest - WriteJson") {
        Lsp::CompletionRequest req;
        req.textDocument.uri = "file:///completion.txt";
        req.position.line = 5;
        req.position.character = 10;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // HoverRequest WriteJson Test
    //=========================================================================

    TEST_CASE("HoverRequest - WriteJson") {
        Lsp::HoverRequest req;
        req.textDocument.uri = "file:///hover.txt";
        req.position.line = 7;
        req.position.character = 12;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // DocumentSymbolRequest WriteJson Test
    //=========================================================================

    TEST_CASE("DocumentSymbolRequest - WriteJson") {
        Lsp::DocumentSymbolRequest req;
        req.textDocument.uri = "file:///symbols.cpp";
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // SignatureHelpRequest WriteJson Test
    //=========================================================================

    TEST_CASE("SignatureHelpRequest - WriteJson") {
        Lsp::SignatureHelpRequest req;
        req.textDocument.uri = "file:///signature.cpp";
        req.position.line = 8;
        req.position.character = 15;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // TextDocumentItem WriteJson Test
    //=========================================================================

    TEST_CASE("TextDocumentItem - WriteJson") {
        Lsp::TextDocumentItem item;
        item.uri = "file:///item.txt";
        item.languageId = "cpp";
        item.version = 1;
        item.text = "int main() {}";
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&item, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(result.find("uri") != std::string::npos);
        CHECK(result.find("languageId") != std::string::npos);
    }

    //=========================================================================
    // DidOpenTextDocumentNotification WriteJson Test
    //=========================================================================

    TEST_CASE("DidOpenTextDocumentNotification - WriteJson") {
        Lsp::DidOpenTextDocumentNotification notif;
        notif.textDocument.uri = "file:///open.txt";
        notif.textDocument.languageId = "text";
        notif.textDocument.version = 1;
        notif.textDocument.text = "content";
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // DidCloseTextDocumentNotification WriteJson Test
    //=========================================================================

    TEST_CASE("DidCloseTextDocumentNotification - WriteJson") {
        Lsp::DidCloseTextDocumentNotification notif;
        notif.textDocument.uri = "file:///close.txt";
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // InitializeRequest WriteJson Test
    //=========================================================================

    TEST_CASE("InitializeRequest - WriteJson") {
        Lsp::InitializeRequest req;
        req.clientInfo.name = "test-client";
        
        char buffer[4096] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // ShutdownRequest WriteJson Test
    //=========================================================================

    TEST_CASE("ShutdownRequest - WriteJson") {
        Lsp::ShutdownRequest req;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&req, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // InitializedNotification WriteJson Test
    //=========================================================================

    TEST_CASE("InitializedNotification - WriteJson") {
        Lsp::InitializedNotification notif;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // ExitNotification WriteJson Test
    //=========================================================================

    TEST_CASE("ExitNotification - WriteJson") {
        Lsp::ExitNotification notif;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // SetTraceParams WriteJson Test
    //=========================================================================

    TEST_CASE("SetTraceParams - WriteJson") {
        Lsp::SetTraceParams params;
        params.value = Lsp::TraceValue_Messages;
        
        char buffer[256] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&params, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // CompletionServerCapabilities ReadJson Test
    //=========================================================================

    TEST_CASE("CompletionServerCapabilities - ReadJson") {
        const char* jsonStr = R"({
            "resolveProvider": true,
            "completionItem": {}
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::CompletionServerCapabilities caps;
        ReadJson(json, &caps);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // CompletionResponse ReadJson Test
    //=========================================================================

    TEST_CASE("CompletionResponse - ReadJson with valid object") {
        const char* jsonStr = R"({
            "isIncomplete": false,
            "items": []
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::CompletionResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // HoverServerCapabilities ReadJson Test
    //=========================================================================

    TEST_CASE("HoverServerCapabilities - ReadJson") {
        const char* jsonStr = R"({
            "workDoneProgress": true
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::HoverServerCapabilities caps;
        ReadJson(json, &caps);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // HoverResponse ReadJson Test
    //=========================================================================

    TEST_CASE("HoverResponse - ReadJson") {
        const char* jsonStr = R"({
            "contents": {
                "kind": "plaintext",
                "value": "Hover info"
            }
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::HoverResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // DocumentSymbolServerCapabilities ReadJson Test
    //=========================================================================

    TEST_CASE("DocumentSymbolServerCapabilities - ReadJson") {
        const char* jsonStr = R"({
            "workDoneProgress": true
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::DocumentSymbolServerCapabilities caps;
        ReadJson(json, &caps);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // SymbolInformation ReadJson Test
    //=========================================================================

    TEST_CASE("SymbolInformation - ReadJson") {
        const char* jsonStr = R"({
            "name": "mySymbol",
            "kind": 5,
            "location": {
                "uri": "file:///test.txt",
                "range": {
                    "start": {"line": 0, "character": 0},
                    "end": {"line": 1, "character": 1}
                }
            }
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::SymbolInformation symInfo;
        ReadJson(json, &symInfo);
        
        CHECK(symInfo.name == "mySymbol");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // DocumentSymbolResponse ReadJson Test
    //=========================================================================

    TEST_CASE("DocumentSymbolResponse - ReadJson with empty list") {
        const char* jsonStr = R"([])";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::DocumentSymbolResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // SignatureHelpServerCapabilities ReadJson Test
    //=========================================================================

    TEST_CASE("SignatureHelpServerCapabilities - ReadJson") {
        const char* jsonStr = R"({
            "workDoneProgress": true
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::SignatureHelpServerCapabilities caps;
        ReadJson(json, &caps);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // SignatureHelpResponse ReadJson Test
    //=========================================================================

    TEST_CASE("SignatureHelpResponse - ReadJson") {
        const char* jsonStr = R"({
            "signatures": []
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::SignatureHelpResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // TextDocumentSyncServerCapabilities ReadJson Test
    //=========================================================================

    TEST_CASE("TextDocumentSyncServerCapabilities - ReadJson") {
        const char* jsonStr = R"({
            "openClose": true,
            "change": 1
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::TextDocumentSyncServerCapabilities caps;
        ReadJson(json, &caps);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // PublishDiagnosticsNotification ReadJson Test
    //=========================================================================

    TEST_CASE("PublishDiagnosticsNotification - ReadJson") {
        const char* jsonStr = R"({
            "uri": "file:///diagnostics.txt",
            "diagnostics": []
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::PublishDiagnosticsNotification notif;
        ReadJson(json, &notif);
        
        CHECK(notif.uri == "file:///diagnostics.txt");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // LogTraceNotification ReadJson Test
    //=========================================================================

    TEST_CASE("LogTraceNotification - ReadJson") {
        const char* jsonStr = R"({
            "message": "Trace message",
            "verbose": "Additional details"
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::LogTraceNotification notif;
        ReadJson(json, &notif);
        
        CHECK(notif.message == "Trace message");
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // InitializeResponse ReadJson Test
    //=========================================================================

    TEST_CASE("InitializeResponse - ReadJson") {
        const char* jsonStr = R"({
            "capabilities": {},
            "serverInfo": {
                "name": "TestServer"
            }
        })";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::InitializeResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // ShutdownResponse ReadJson Test
    //=========================================================================

    TEST_CASE("ShutdownResponse - ReadJson") {
        const char* jsonStr = R"(null)";
        cJSON* json = cJSON_Parse(jsonStr);
        
        Lsp::ShutdownResponse response;
        ReadJson(json, &response);
        
        // Just ensure it doesn't crash on valid input
        
        cJSON_Delete(json);
    }

    //=========================================================================
    // CompletionClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("CompletionClientCapabilities - WriteJson") {
        Lsp::CompletionClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // HoverClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("HoverClientCapabilities - WriteJson") {
        Lsp::HoverClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // DocumentSymbolClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("DocumentSymbolClientCapabilities - WriteJson") {
        Lsp::DocumentSymbolClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // SignatureHelpClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("SignatureHelpClientCapabilities - WriteJson") {
        Lsp::SignatureHelpClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // TextDocumentSyncClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("TextDocumentSyncClientCapabilities - WriteJson") {
        Lsp::TextDocumentSyncClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // GotoReferencesClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("GotoReferencesClientCapabilities - WriteJson") {
        Lsp::GotoReferencesClientCapabilities caps;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // ShowMessageRequestClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("ShowMessageRequestClientCapabilities - WriteJson") {
        Lsp::ShowMessageRequestClientCapabilities caps;
        
        char buffer[1024] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // PublishDiagnosticsClientCapabilities WriteJson Test
    //=========================================================================

    TEST_CASE("PublishDiagnosticsClientCapabilities - WriteJson") {
        Lsp::PublishDiagnosticsClientCapabilities caps;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&caps, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // DidChangeTextDocumentNotification WriteJson Test
    //=========================================================================

    TEST_CASE("DidChangeTextDocumentNotification - WriteJson") {
        Lsp::DidChangeTextDocumentNotification notif;
        notif.textDocument.uri = "file:///change.txt";
        notif.textDocument.version = 2;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // WillSaveTextDocumentNotification WriteJson Test
    //=========================================================================

    TEST_CASE("WillSaveTextDocumentNotification - WriteJson") {
        Lsp::WillSaveTextDocumentNotification notif;
        notif.textDocument.uri = "file:///willsave.txt";
        notif.saveReason = Lsp::WillSaveTextDocumentNotification::SaveReason_Manual;
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

    //=========================================================================
    // DidSaveTextDocumentNotification WriteJson Test
    //=========================================================================

    TEST_CASE("DidSaveTextDocumentNotification - WriteJson") {
        Lsp::DidSaveTextDocumentNotification notif;
        notif.textDocument.uri = "file:///save.txt";
        
        char buffer[2048] = {};
        JsonWriteBuffer writeBuffer {buffer};
        WriteJson(&notif, &writeBuffer);
        
        std::string result(writeBuffer.GetString());
        CHECK(!result.empty());
    }

}
