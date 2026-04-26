#include "file-util.hh"
#include "basic.hh"
#include "util/logging.hh"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <charconv>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool ReadEntireFile(std::string_view path, /*out*/ std::string* buffer) {

	HANDLE hFile = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		LogError("failed to open file '%'. LastError: %", path, FLastErr(GetLastError()));
		return false;
	}

	DWORD hiSize = 0;
	const DWORD loSize = GetFileSize(hFile, &hiSize);
	if (loSize == INVALID_FILE_SIZE) {
		LogError("failed to obtain file size of '%'. LastError: %", path, FLastErr(GetLastError()));
		CloseHandle(hFile);
		return false;
	}

	const u64 size = (hiSize << 4) | loSize;
	ASSERT(size < UINT32_MAX);
	
	buffer->resize(size);

	DWORD bytesRead = 0;
	if (!ReadFile(hFile, buffer->data(), static_cast<u32>(size), &bytesRead, NULL)) {
		LogError("ReadFile() failed. LastError: %", FLastErr(GetLastError()));
		CloseHandle(hFile);
		return false;
	}

	CloseHandle(hFile);
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
				LogError("error converting path to uri: %\nfailed to encode characetrer '%' (%), Error: %",
					orgPath, charaterToEncode, static_cast<int>(charaterToEncode), F(toCharsResult));
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
				LogWarning("unexpected '%' in uri %. % chars before the end", '%', orgUri, pos);
				continue;
			}
			
			char characterToInsert = '\0';
			const std::from_chars_result fromCharsRes = std::from_chars(uri.data() + pos + 1u, uri.data() + pos + 3u, characterToInsert, 16);
			
			if (fromCharsRes.ec == std::errc()) {
				path.push_back(characterToInsert);
				
			} else {
				LogError("error converting uri to path: %\nfailed to decode special char '%'. Error: %", 
					orgUri,
					uri.substr(pos, 3u),
					F(fromCharsRes));
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

std::string GetProcessDirectory() {

	std::string buffer(MAX_PATH, '\0');
	const u32 len = GetModuleFileNameA(NULL, buffer.data(), MAX_PATH);
	ASSERT(len != ERROR_INSUFFICIENT_BUFFER);
	
	return buffer;
}

std::string_view GetWorkingDirectory() {
	static char cwdBuffer[_MAX_PATH + 1u];
	const u64 cwdLen = GetCurrentDirectoryA(_MAX_PATH + 1u, cwdBuffer);
	
	return std::string_view {cwdBuffer, cwdLen};
}

static bool PathsAreEquivalentInternal(std::string_view pathA, std::string_view pathB) {
	
	const std::string_view cwd = GetWorkingDirectory();
	if (pathA.starts_with(".\\")) {
		
		// match .\ path and absolute path
		if (pathB.starts_with(cwd) && pathA.substr(2) == pathB.substr(cwd.length()))
		 	return true;
	
		// match .\ path with relative path (without .\)
		if (pathA.substr(2) == pathB)
			return true;
	}
	
	// match relative path with absolute path
	if (pathB.starts_with(cwd) && pathA == pathB.substr(cwd.length()))
	 	return true;
	 
	 return false;
}

bool PathsAreEquivalent(std::string_view pathA, std::string_view pathB) {
	if (pathA == pathB) return true;
	if (PathsAreEquivalentInternal(pathA, pathB)) return true;
	if (PathsAreEquivalentInternal(pathB, pathA)) return true;
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool DirectoryIterator::Next() {

	WIN32_FIND_DATAA findData {};

try_next:
	if (hHandle == INVALID_HANDLE_VALUE) {
		ASSERT(lastError != 0u);
		return false;
	
	} else if (hHandle == NULL) {
		hHandle = FindFirstFileA(searchPattern.c_str(), &findData);
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
	fileSize = (findData.nFileSizeHigh << 4) | (findData.nFileSizeLow);
	
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
	return std::string_view {searchPattern.begin(), searchPattern.end()-2};
}

DirectoryIterator::DirectoryIterator(std::string searchPath) noexcept {
	if (!searchPath.ends_with('\\'))
		 searchPath.push_back('\\');

	searchPath.push_back('*');
	this->searchPattern = std::move(searchPath);
}

DirectoryIterator::~DirectoryIterator() noexcept {
	FindClose(hHandle);
}
