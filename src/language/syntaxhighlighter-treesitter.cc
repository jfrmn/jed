#include "syntaxhighlighter-treesitter.hh"
#include "settings.hh"
#include "editor/editor.hh"
#include "graphics/effects.hh"
#include "logging.hh"
#include "util.hh"

#include <tree_sitter/api.h>

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

bool SyntaxHighlighterTreeSitter::FromToml(toml::node* toml) {
	toml::table* table = toml->as_table();	
	if (!table) {
		LogError("%s: expected a table", Str(toml->source()));
		return false;
	}
	
	auto valLibrary = table->get_as<std::string>("library");
	if (!valLibrary) {
		LogError("%s: expected entry 'library' as string", Str(table->source()));
		return false;
	}
	this->modulePath = std::move(valLibrary->get());
	
	auto valFunctionName  = table->get_as<std::string>("function-name");
	if (!valFunctionName) {
		this->modulePath.clear();
		LogError("%s: expected entry 'function-name' as string", Str(table->source()));
		return false;
	}
	this->procName = std::move(valFunctionName->get());
	
	auto valQueryFile = table->get_as<std::string>("query-file");
	auto valQuery = table->get_as<std::string>("query");
	
	if (valQueryFile && valQuery) {
		LogInfo("both 'query-file' and 'query' specified. Concating...");
		
		if (!ReadEntireFile(valQueryFile->get(), &this->queryText))
			LogError("%s: failed to read query file", Str(valQueryFile->source()));
		
		this->queryText.reserve(valQuery->get().size() + 1u);
		this->queryText.push_back('\n');
		this->queryText.append(valQuery->get());
			
	} else if (valQueryFile) {
		if (!ReadEntireFile(valQueryFile->get(), &this->queryText))
			LogError("%s: failed to read query file", Str(valQueryFile->source()));
		
	} else if (valQuery) {
		this->queryText = std::move(valQuery->get());
	
	} else {
		LogWarning("%s: no query specified", Str(table->source()));
	}
		
	return true;
}

SyntaxHighlighterTreeSitter::~SyntaxHighlighterTreeSitter() noexcept {
	if (hModule) FreeLibrary(static_cast<HMODULE>(hModule));
	language = nullptr;
	hModule = nullptr;
}

std::string_view GetQueryErrorAsStr(TSQueryError error) {
	switch (error) {
		case TSQueryErrorNone: return "TSQueryErrorNone";
		case TSQueryErrorSyntax: return "TSQueryErrorSyntax";
		case TSQueryErrorNodeType: return "TSQueryErrorNodeType";
		case TSQueryErrorField: return "TSQueryErrorField";
		case TSQueryErrorCapture: return "TSQueryErrorCapture";
		case TSQueryErrorStructure: return "TSQueryErrorStructure";
		case TSQueryErrorLanguage: return "TSQueryErrorLanguage";
		default: return "unknown";
	}
}

static bool Init(SyntaxHighlighterTreeSitter* self) {
	ASSERT(!self->language);
	ASSERT(!self->query);
	ASSERT(!self->queryCursor);
	
	HMODULE hModule = LoadLibraryA(self->modulePath.c_str());
	if (!hModule) {
		LogError("failed to load library: '%s'. Last Error: %s", self->modulePath.c_str(), StrLastErr(GetLastError()));
		return false;
	}
	
	FARPROC procAddr = GetProcAddress(hModule, self->procName.c_str());
	if (!procAddr) {
		LogError("failed to get proc address: '%s'. Last Error: %s", self->procName.c_str(), StrLastErr(GetLastError()));
		FreeLibrary(hModule);
		return false;
	}
	
	// @TODO check ABI compability
	auto GetLanguage = (const TSLanguage* (*)())(procAddr);
	self->hModule = hModule;
	self->language = GetLanguage();
	
	if (self->queryText.empty()) return false;
	
	u32 errorOffset = 0u; TSQueryError error = TSQueryErrorNone;
	self->query = ts_query_new(self->language, self->queryText.data(), static_cast<u32>(self->queryText.size()), &errorOffset, &error);
	if (!self->query) {
		LogError("failed to create tree-sitter query: %s at offset %u", GetQueryErrorAsStr(error), errorOffset);
		return false;
	}
	
	self->queryCursor = ts_query_cursor_new();
	if (!self->queryCursor) return false;
	
	return true;
}

void SyntaxHighlighterTreeSitter::OnOpenFile(Editor* editor, std::string_view buffer) {
	if (modulePath.empty() || procName.empty()) return;
	
	if (!language && !Init(this)) return;
		
	editor->tsParser = ts_parser_new();
	if (!editor->tsParser) {
		LogError("failed to create tree-sitter parser.");
		return;
	}
	
	if (!ts_parser_set_language(editor->tsParser, language)) {
		LogError("failed to set language to tree-sitter parser");
		return;
	}
	 
	editor->tsTree = ts_parser_parse_string_encoding(editor->tsParser, nullptr, buffer.data(), static_cast<u32>(buffer.size()), TSInputEncodingUTF8);
	if (!editor->tsTree) {
		LogError("failed parse tree-sitter tree");
		return;
	}
}

void SyntaxHighlighterTreeSitter::OnCloseFile(Editor* editor) {
	
	ts_parser_delete(editor->tsParser);
	editor->tsParser = nullptr;
	
	ts_tree_delete(editor->tsTree);
	editor->tsTree = nullptr;
}

static TSPoint ToTsPoint(TextPosition pt) {
	return TSPoint {
		.row    = static_cast<u32>(pt.line),
		.column = static_cast<u32>(pt.column)};
}

static const char* Str(const TSInputEdit& input) {
	static char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	sprintf_s(buffer, "Edit:\n"
		"sb: %u oeb: %u neb: %u\n"
		"sp: %u:%u\n"
		"oep: %u:%u\n"
		"nep: %u:%u",
         input.start_byte, input.old_end_byte, input.new_end_byte,
		 input.start_point.row, input.start_point.column,
		 input.old_end_point.row, input.old_end_point.column,
		 input.new_end_point.row, input.new_end_point.column);
	return buffer;
}

#define DEBUG_LOG_TREESITTER 0

void SyntaxHighlighterTreeSitter::OnTextBufferChanged(Editor* editor, const TextChange* change) {
	TextBuffer& buffer = editor->GetBuffer();
	
	for (u64 iop = 0u; iop < change->count; iop++) {
		const TextChangeOperation& operation = change->operations[iop];
		
		const TSPoint startPoint = ToTsPoint(operation.start);
		
		u64 startLineByte = 0u;
		ASSERT(operation.start.line < buffer.LineCount());
		for (u64 i = 0u; i < operation.start.line; i++) {
			const TextBuffer::Line& line = buffer.GetLineAt(i);
			startLineByte += line.LengthWithLinebreak();
		}
		const u64 startByte = startLineByte + operation.start.column;
		
		if (!operation.insertedText.empty()) {
			
			const TSInputEdit tsEdit {
				.start_byte = static_cast<u32>(startByte),
				.old_end_byte = static_cast<u32>(startByte),
				.new_end_byte = static_cast<u32>(startByte + operation.insertedText.size()),
				.start_point = startPoint,
				.old_end_point = startPoint,
				.new_end_point = ToTsPoint(operation.insertionEnd)};
			ts_tree_edit(editor->tsTree, &tsEdit);
#if DEBUG_LOG_TREESITTER
			LogDev("edit: %s", Str(tsEdit));	
#endif		
		}
		
		
		if (!operation.removedText.empty()) {
		
			const TSInputEdit tsEdit {
				.start_byte = static_cast<u32>(startByte),
				.old_end_byte = static_cast<u32>(startByte + operation.removedText.size()),
				.new_end_byte = static_cast<u32>(startByte),
				.start_point = startPoint,
				.old_end_point = ToTsPoint(operation.removalEnd),
				.new_end_point = startPoint};
			ts_tree_edit(editor->tsTree, &tsEdit);
#if DEBUG_LOG_TREESITTER
			LogDev("edit: %s", Str(tsEdit));
#endif
		}
	}
	
	const TSInput tsInput {
		.payload = &buffer,
		.read = [] (void* payload, u32 byteIndex, TSPoint position, u32* bytesRead) -> const char* {
			
			auto buffer = static_cast<const TextBuffer*>(payload);
			ASSERT(position.row < buffer->LineCount());
			
			const TextBuffer::Line& line = buffer->GetLineAt(position.row);
			ASSERT(position.column <= line.LengthWithLinebreak());
			
			*bytesRead = static_cast<u32>(line.LengthWithLinebreak() - position.column);
			
			const char* result = line.data + position.column;
		
#if DEBUG_LOG_TREESITTER
			const std::string_view resultForLogging {result, *bytesRead};
			LogDev("read at %u:%u (b %u) -> %u b \"%.*s\"", position.row, position.column, byteIndex, *bytesRead, SIZE_AND_DATA(resultForLogging));
#endif
			return result;
		},
		.encoding = TSInputEncodingUTF8,
		.decode = nullptr};
	
	editor->tsTree = ts_parser_parse(editor->tsParser, editor->tsTree, tsInput);
}

// @DUMMY
static D2D1_COLOR_F GetColorForLabel(std::string_view label) {
	if (label == "keyword")
		return D2D1::ColorF(D2D1::ColorF::RoyalBlue);
	else if (label == "function")
		return D2D1::ColorF(D2D1::ColorF::LemonChiffon);
	else if (label == "control-flow")
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
	else if (label == "tag")
		return D2D1::ColorF(D2D1::ColorF::Gold);
	else if (label == "normal")
		return settings.colors.editorText.ToD2D();
	else
		return D2D1::ColorF(D2D1::ColorF::White);
}

void SyntaxHighlighterTreeSitter::Highlight(Editor* editor, ID2D1RenderTarget* renderTarget, u64 fromLine, u64 toLine) {
	if (!query || !queryCursor) return;
	if (!editor->tsTree) return;
	
	ts_query_cursor_set_point_range(
		queryCursor,
		TSPoint {.row = static_cast<u32>(fromLine),  .column = 0u},
		TSPoint {.row = static_cast<u32>(toLine+1u), .column = 0u});
	
	const TSNode rootNode = ts_tree_root_node(editor->tsTree);
	ts_query_cursor_exec(queryCursor, query, rootNode);
	
	TSQueryMatch tsMatch {}; u32 caputeIndex = 0u;
	while (ts_query_cursor_next_capture(queryCursor, &tsMatch, &caputeIndex)) {
		const TSQueryCapture& capture = tsMatch.captures[caputeIndex];
		
		u32 captureNameLength = 0u;
		const char* captureNameData = ts_query_capture_name_for_id(query, capture.index, &captureNameLength);
		
		const std::string_view captureName {captureNameData, captureNameLength};
		const D2D1_COLOR_F color = GetColorForLabel(captureName);
		brush->SetColor(color);
		
		const TSPoint start = ts_node_start_point(capture.node);
		TSPoint end = ts_node_end_point(capture.node);
		
		// @TODO we need to make the IterateGlyphRange function in the editor available
		// but for now we just use this hack here
		if (end.row != start.row) {
			end.row =  start.row;
			end.column = static_cast<u32>(editor->GetBuffer().GetLineAt(start.row).length);
		}
	
		const GlyphRun& run = editor->glyphRuns[start.row];
		
		f32 offsetStart, offsetEnd;
		run.MeasureOffsetRange(start.column, end.column, &offsetStart, &offsetEnd);
		
		renderTarget->FillRectangle(
			D2D_RECT_F {
				.left   = offsetStart,
				.top    = (settings.fontEditor.lineHeight * start.row),
				.right  = offsetEnd,
				.bottom = (settings.fontEditor.lineHeight * (start.row+1)) },
			brush);
	}
}
