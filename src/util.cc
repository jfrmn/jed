#include "util.hh"
#include "logging.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d2d1.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helpers for Rects, Points and Size
//
///////////////////////////////////////////////////////////////////////////////////////////////////

D2D_POINT_2F PointOffset(const D2D_POINT_2F& pt, f32 dx, f32 dy) {
	return D2D_POINT_2F {
		.x = pt.x + dx,
		.y = pt.y + dy};
}
D2D_POINT_2F PointOffsetX(const D2D_POINT_2F& pt, f32 dx) {
	return D2D_POINT_2F {
		.x = pt.x + dx,
		.y = pt.y};
}

D2D_POINT_2F PointOffsetY(const D2D_POINT_2F& pt, f32 dy) {
	return D2D_POINT_2F {
		.x = pt.x,
		.y = pt.y + dy};
}

D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, const D2D_SIZE_F& s) {
	return D2D_RECT_F {
		.left   = tl.x,
		.top    = tl.y,
		.right  = tl.x + s.width,
		.bottom = tl.y + s.height};
}
D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, f32 w, f32 h) {
	return D2D_RECT_F {
		.left   = tl.x,
		.top    = tl.y,
		.right  = tl.x + w,
		.bottom = tl.y + h};
}
D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, const D2D_POINT_2F& br) {
	return D2D_RECT_F {
		.left   = tl.x,
		.top    = tl.y,
		.right  = br.x,
		.bottom = br.y};
}

D2D_RECT_F MakeRect(f32 x, f32 y, f32 w, f32 h) {
	return D2D_RECT_F {
		.left   = x,
		.top    = y,
		.right  = x + w,
		.bottom = y + h};
}

f32 RectWidth(const D2D_RECT_F& rect) {
	return rect.right - rect.left;
}

f32 RectHeight(const D2D_RECT_F& rect) {
	return rect.bottom - rect.top;
}

D2D_SIZE_F RectSize(const D2D_RECT_F& rect) {
	return D2D_SIZE_F {
		.width  = RectWidth(rect),
		.height = RectHeight(rect)};
}

bool RectContains(const D2D_RECT_F& rect, const D2D_POINT_2F& pt) {
	return pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom;
}

bool RectContains(const D2D_RECT_F& rect, f32 x, f32 y) {
	return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

D2D1_ROUNDED_RECT ToRounded(const D2D_RECT_F& rect, f32 radius /*= 5.0f*/) {
	return D2D1_ROUNDED_RECT {
		.rect = rect,
		.radiusX = radius,
		.radiusY = radius};
}

D2D_SIZE_U ToU(const D2D_SIZE_F& size) {
	return D2D_SIZE_U {
		.width  = static_cast<u32>(size.width),
		.height = static_cast<u32>(size.height)};
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// String helpers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool ToUtf16(std::string_view utf8, /*out*/ std::span<wchar> utf16Buf, /*out*/ u64* len) {
	
	if (utf16Buf.empty()) {
	
		const int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), NULL, 0);
		if (requiredSize == 0)
			return false;

		ASSERT(len);
		*len = requiredSize;
		return true;
	
	} else {
			
		const int result = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), utf16Buf.data(), static_cast<int>(utf16Buf.size()));
		if (result == 0)
			return false;
		
		if (len)
		   *len = result;
			
		return true;
	}
}

bool ToUtf8(std::wstring_view utf16, /*out*/ std::span<char> utf8Buf, /*out*/ u64* len) {
	
	if (utf8Buf.empty()) {
	
		const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), NULL, 0, NULL, NULL);
		if (requiredSize == 0)
			return false;

		ASSERT(len);
		*len = requiredSize;
		return true;
	
	} else {
			
		const int result = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), utf8Buf.data(), static_cast<int>(utf8Buf.size()), NULL, NULL);
		if (result == 0)
			return false;
		
		if (len)
           *len = result;
			
		return true;
	}
}

bool IsMultibyteCodepointMember(char ch) {
	return (ch & 0xC0) == 0x80;
}

static void FormatStringInternal(std::string* str, const char* fmt, va_list args) {
	char buffer[64] {0};
	const int maxCharaceters = sizeof(buffer) - 1u;
	const int len = vsnprintf(buffer, maxCharaceters, fmt, args);
	if (len > maxCharaceters) {
		str->resize(static_cast<u64>(len + 1)); // need one more for the null-terminator
		vsprintf_s(str->data(), str->size(), fmt, args);
		str->pop_back(); // no more need for the null terminator
	
	} else {
		str->resize(static_cast<u64>(len));
		memcpy(str->data(), buffer, len);
	}
}

std::string FormatString(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result;
	FormatStringInternal(&result, fmt, args);
	va_end(args);
	return result;
}

void FormatString(std::string* str, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	FormatStringInternal(str, fmt, args);
	va_end(args);
}

bool StringEqualsCaseInsen(std::string_view lhs, std::string_view rhs) {
	if (lhs.size() != rhs.size()) return false;

	for (u64 i = 0; i < lhs.size(); i++) {
		if (tolower(lhs[i]) != tolower(rhs[i]))
			return false;
	}
	return true;
}

bool StringEqualsCaseInsen(std::string_view lhs, std::wstring_view rhs) {
	if (lhs.size() != rhs.size()) return false;

	for (u64 i = 0; i < lhs.size(); i++) {
		if (lhs[i] >= 128 || rhs[i] >= 128) return false;
		if (tolower(lhs[i]) != tolower(static_cast<char>(rhs[i])))
			return false;
	}
	return true;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Fuzzy Matching
//
///////////////////////////////////////////////////////////////////////////////////////////////////
bool FuzzyMatch(std::string_view pattern, std::string_view fullText, /*out*/ FuzzyMatchResult* result) {
	ASSERT(!fullText.empty());
	ASSERT(result);

	if (fullText.size() < pattern.size())
		return false;
	
	ASSERT(!pattern.empty());
	for (u64 i = 0; i <= fullText.size() - pattern.size(); i++) {
		
		u64 currentMatchedChars = 0u;		
		for (u64 j = 0; j < pattern.size(); j++) {
			
			// getting rid of C4334
			//constexpr u64 ONE = 1u;
			
			if (fullText[i + j] == pattern[j])
				currentMatchedChars++;
		}
		
		if (result->matchedCount < currentMatchedChars) {
			result->matchedCount = currentMatchedChars;
			result->position = i;
			result->length = pattern.size();
		}
	}
	
	return (result->matchedCount > 0);
}

int CompareFuzzyMatchResults(const FuzzyMatchResult& lhs, const FuzzyMatchResult& rhs) {
	if (lhs.matchedCount > rhs.matchedCount) return  1;
	if (lhs.matchedCount < rhs.matchedCount) return -1;
			
	if (lhs.position < rhs.position) return  1;
	if (lhs.position > rhs.position) return -1;
	
	if (lhs.length < rhs.length) return  1;
	if (lhs.length > rhs.length) return -1;
			
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File and Path helpers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool ReadEntireFile(std::string_view path, /*out*/ std::string* buffer, /*out*/ FILETIME* lastWriteTime) {

	HANDLE hFile = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%.*s'. LastError: %s", SIZE_AND_DATA(path), StrLastErr(GetLastError()));
		return false;
	}
	DEFER(CloseHandle(hFile));

	s64 fileSize = 0; 
	{
		LARGE_INTEGER liFileSize {0};
		if (!GetFileSizeEx(hFile, &liFileSize)) {
			LogError("failed to obtain file size of '%.*s'. LastError: %s", SIZE_AND_DATA(path), StrLastErr(GetLastError()));
			return false;
		}

		fileSize = liFileSize.QuadPart;
	}

	// @ISSUE
	// File size is a s64 but ReadFile only takes a DWORD aka u32
	// Does this mean we can only read 4GB in one go?
	ASSERT(fileSize < U32_MAX);
	
	buffer->resize(fileSize);

	DWORD bytesRead = 0;
	if (!ReadFile(hFile, buffer->data(), static_cast<u32>(fileSize), &bytesRead, NULL)) {
		LogError("ReadFile() failed. LastError: %", StrLastErr(GetLastError()));
		return false;
	}

	if (lastWriteTime)
		GetFileTime(hFile, nullptr, nullptr, lastWriteTime);

	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static constexpr std::string_view URI_FILE_SCHEMA = "file:///";
static constexpr std::string_view URI_UNRESERVED_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_~./";

std::string MakeUriFromPath(std::string_view path) {
	const std::string_view orgPath = path; // for logging

	if (path.size() < 2)
		return std::string {"ERROR: path.size() < 2"};

	std::string uri {URI_FILE_SCHEMA};
	
	// relative path?
	if (path[1] != ':') {
		
		char cwd[_MAX_PATH];
		const u64 length = GetCurrentDirectoryA(_MAX_PATH, cwd);
		uri.reserve(length + 1);	
		
		// windows of course provides backslashes but the uri
		// should contain only forward slashes
		for (u64 i = 0u; i < length; i++)
			if (cwd[i] == '\\') cwd[i] = '/';
		
		// append the current working directory
		uri.append(std::string_view {cwd, length});
		
		// GetCurrentDirectory doesn't provide a trailing slash
		uri.push_back('/');
		
		// if the path starts with a dot slash ("./example.txt") we can remove the dot
		// We need to check for the slash however -- otherwise it might be a dotfile (".gitignore")
		if (path[0] == '.' && (path[1] == '/' || path[1] == '\\'))
			path.remove_prefix(2);
	}
	
	// encode special charaters (e.g. space = %20)
	u64 posReservedCharaceter = path.find_first_not_of(URI_UNRESERVED_CHARACTERS);
	while (posReservedCharaceter != std::string_view::npos) {
		
		uri.append(path.substr(0u, posReservedCharaceter));
		
		const char charaterToEncode = path[posReservedCharaceter];
		if (charaterToEncode == '\\') {
			uri.push_back('/');
		
		} else {
			char buffer[2] {'\0'};
			const std::to_chars_result toCharsResult = std::to_chars(buffer, buffer + 2u, charaterToEncode, 16);
			if (toCharsResult.ec == std::errc()) {
				uri.push_back('%');
				uri.append(buffer, buffer + 2u);
			} else {
				uri.push_back(charaterToEncode);
				LogError("error converting path to uri: %.*s\nfailed to encode characetrer '%c' (%d), Error: %s",
					(int)orgPath.size(), orgPath.data(), charaterToEncode, static_cast<int>(charaterToEncode), Str(toCharsResult));
			}
		}
		
		path.remove_prefix(posReservedCharaceter + 1u);
		posReservedCharaceter = path.find_first_not_of(URI_UNRESERVED_CHARACTERS);
	}
	
	// append the rest
	uri.append(path);
		
	return uri;

}

std::string MakePathFromUri(std::string_view uri) {
	const std::string_view orgUri = uri; // for logging
	
	std::string path {};
	
	if (uri.starts_with(URI_FILE_SCHEMA)) {
		uri.remove_prefix(URI_FILE_SCHEMA.size());
	}
	
	u64 pos = uri.find_first_of("%/");
	while (pos != std::string_view::npos) {
		
		// append anything before the found char
		path.append(uri.substr(0u, pos));
				
		if (uri[pos] == '%') {
			
			if (pos > uri.size() - 3u) {
				LogWarning("unexpected '%' in uri %.*s. %zu chars before the end", SIZE_AND_DATA(orgUri), pos);
				continue;
			}
			
			char characterToInsert = '\0';
			const std::from_chars_result fromCharsRes = std::from_chars(uri.data() + pos + 1u, uri.data() + pos + 3u, characterToInsert, 16);
			
			if (fromCharsRes.ec == std::errc()) {
				path.push_back(characterToInsert);
				
			} else {
				LogError("error converting uri to path: %.*s\nfailed to decode special char '%.*s'. Error: %s", 
					SIZE_AND_DATA(orgUri),
					3, uri.data() + pos,
					Str(fromCharsRes));
				path.append(uri.substr(pos, 3u));
			}
			
			uri.remove_prefix(pos + 3u);
		
		} else if (uri[pos] == '/') {
			path.push_back('\\');
			uri.remove_prefix(pos + 1u);
		}
		
		pos = uri.find_first_of("%/");
	}
	
	// append rest
	path.append(uri);
	
	// if the path starts with the current directory, make it a relative path
	char cwd[_MAX_PATH];
	const u64 length = GetCurrentDirectoryA(_MAX_PATH, cwd);
	
	if (length > 1u && memcmp(cwd, path.data(), length * sizeof(char)) == 0) {
		path.erase(path.begin(), path.begin() + length - 1u);
		path[0] = '.';
	}
	
	return path;
}

std::string_view GetFilenameFromPath(std::string_view path) {
	
	const size_t posDelimiter = path.find_last_of("/\\");
	if (posDelimiter == std::string::npos) {
		return path;
	}

	return path.substr(posDelimiter + 1);
}

std::string_view GetExtensionFromPath(std::string_view path) {
	
	const size_t posDelimiter = path.rfind(".");
	if (posDelimiter == std::string::npos) {
		return path;
	}

	return path.substr(posDelimiter);
}

std::string_view GetDirectoryFromPath(std::string_view path) {
	
	const size_t posDelimiter = path.find_last_of("/\\");
	if (posDelimiter == std::string::npos) {
		return std::string_view {};
	}

	return path.substr(0u, posDelimiter);
}

std::string_view GetProcessDirectory() {

	static char buffer[_MAX_PATH];
	const u32 len = GetModuleFileNameA(NULL, buffer, sizeof(buffer));
	ASSERT(len != ERROR_INSUFFICIENT_BUFFER);
	
	return std::string_view {buffer, len};
}

std::string_view GetWorkingDirectory() {
	static char buffer[_MAX_PATH];
	const u32 len = GetCurrentDirectoryA(sizeof(buffer), buffer);
	
	return std::string_view {buffer, len};
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Directory iterator
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool DirectoryIterator::Next() {

	WIN32_FIND_DATAA findData {};

try_next:
	if (hHandle == INVALID_HANDLE_VALUE) {
		ASSERT(lastError != 0u);
		return false;
	
	} else if (hHandle == NULL) {
		ASSERT(strnlen_s(searchPath, _MAX_PATH) < _MAX_PATH);
		
		hHandle = FindFirstFileA(searchPath, &findData);
		if (hHandle == INVALID_HANDLE_VALUE) {
			lastError = GetLastError();
			return false;
		}

	} else {
		const BOOL res = FindNextFileA(hHandle, &findData);
		if (!res) {
			lastError = GetLastError();
			return false;
		}	
	}

	if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
	 	
		if (strcmp(".", findData.cFileName) == 0)
			goto try_next;

		if (strcmp("..", findData.cFileName) == 0)
			goto try_next;
	}

	fileAttributes = findData.dwFileAttributes;
	fileSize = static_cast<u64>(findData.nFileSizeHigh) << 32 | static_cast<u64>(findData.nFileSizeLow);
	
	const usize filenameLength = strlen(findData.cFileName);
	filename.resize(filenameLength);
	memcpy_s(filename.data(), filename.size(), findData.cFileName, filenameLength);
	
	lastError = 0u;
	return true;
}

bool DirectoryIterator::Failed() const {
	return  (lastError != 0u)
		 && (lastError != ERROR_NO_MORE_FILES);
}

bool DirectoryIterator::IsDirectory() const {
	return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DirectoryIterator::IsFile() const {
	return (fileAttributes & FILE_ATTRIBUTE_NORMAL) != 0;
}

std::string_view DirectoryIterator::GetSearchPath() const {
	const u64 len = strlen(searchPath);
	return std::string_view {searchPath, len >= 2 ? len-2 : len};
}

DirectoryIterator::DirectoryIterator() noexcept {}

DirectoryIterator::DirectoryIterator(std::string_view directoryPath) noexcept {
	memcpy(searchPath, directoryPath.data(), directoryPath.size());
	
	char* tail = searchPath + directoryPath.size();
	if (!directoryPath.ends_with('\\'))
		*tail++ = '\\';
	*tail++ = '*';
}

DirectoryIterator::~DirectoryIterator() noexcept {
	FindClose(hHandle);
}
