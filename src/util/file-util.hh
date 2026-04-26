#pragma once
#include "basic.hh"
#include <string>

//---------------------------------------------------------
// File operations
//---------------------------------------------------------

// read an entire file
bool ReadEntireFile(std::string_view path, /*out*/ std::string* buffer);

// @DEPRICATED not used anymore
// determine if to filepaths point to the same item in the filesystem
bool IsSameFile(std::string_view pathA, std::string_view pathB);

//---------------------------------------------------------
// Uri and Path functions
//---------------------------------------------------------

// takes a path and creates the coresponding file:///-Uri
std::string MakeUriFromPath(std::string_view path);

// takes a file:///-Uri and extracts the filepath from it, decodes special characeter (e.g. %20 = space)
std::string MakePathFromUri(std::string_view uri);

// takes a path and returns the directory/filename/extension
std::string_view GetFilenameFromPath(std::string_view path);
std::string_view GetExtensionFromPath(std::string_view path);
std::string_view GetDirectoryFromPath(std::string_view path);

// tries to check if 2 paths point to the same file
bool PathsAreEquivalent(std::string_view pathA, std::string_view pathB);

// get the direcotry path in which the current .exe resides
// the returned path DOES have a trailing slash
std::string GetProcessDirectory(); // @TODO remove

// returns the current working directory
// path has a trailing slash
// WARNING! uses a static buffer to hold the data
// not threadsafe!
std::string_view GetWorkingDirectory();

//---------------------------------------------------------
// direcotry iterator
//---------------------------------------------------------
// iterate over all files and sub-directories in a dir
struct DirectoryIterator {

	std::string searchPattern = {};

	void* hHandle = nullptr;
	u64 fileSize = 0u;
	u32 fileAttributes = 0u;
	u32 lastError = 0u;
	std::string filename = {};

	bool Next();
	bool Failed() const;

	bool IsDirectory() const;
	bool IsFile() const;
	std::string_view GetSearchPath() const;

	explicit DirectoryIterator(std::string searchPath) noexcept;
	~DirectoryIterator() noexcept;
};

