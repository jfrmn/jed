#pragma once
#include "basic.hh"
#include <string>
#include <string_view>
#include <span>

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Helpers for Rects, Points and Size
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct D2D_POINT_2F;
struct D2D_RECT_F;
struct D2D_SIZE_F;
struct D2D_SIZE_U;
struct D2D1_ROUNDED_RECT;

// offset a point by the passed amount
D2D_POINT_2F PointOffset(const D2D_POINT_2F& pt, f32 dx, f32 dy);
D2D_POINT_2F PointOffsetX(const D2D_POINT_2F& pt, f32 dx);
D2D_POINT_2F PointOffsetY(const D2D_POINT_2F& pt, f32 dy);

// create a rect in various ways
D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, const D2D_SIZE_F& s);
D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, f32 w, f32 h);
D2D_RECT_F MakeRect(const D2D_POINT_2F& tl, const D2D_POINT_2F& br);
D2D_RECT_F MakeRect(f32 x, f32 y, f32 w, f32 h);

f32 RectWidth(const D2D_RECT_F& rect);
f32 RectHeight(const D2D_RECT_F& rect);
D2D_SIZE_F RectSize(const D2D_RECT_F& rect);
bool RectContains(const D2D_RECT_F& rect, const D2D_POINT_2F& pt);
bool RectContains(const D2D_RECT_F& rect, f32 x, f32 y);

D2D1_ROUNDED_RECT ToRounded(const D2D_RECT_F& rect, f32 radius = 5.0f);
D2D_SIZE_U ToU(const D2D_SIZE_F& size);

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// String helpers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

bool ToUtf16(std::string_view utf8,  /*out*/ std::span<wchar> utf16Buf, /*out*/ u64* len);
bool ToUtf8(std::wstring_view utf16, /*out*/ std::span<char> utf8Buf,   /*out*/ u64* len);

// returns true if this byte is part of a utf8 multibyte codepoint, but not the first byte
// returns false if this first byte of utf8 multibyte codepoint
// returns false for ascii/single-byte-character
bool IsMultibyteCodepointMember(char ch);

std::string FormatString(const char* fmt, ...);
void FormatString(/*out*/ std::string* str, const char* fmt, ...);

// compares two strings case-insensitively
// if rhs is a wstring, it will always return false if there is a non-ascii char in any of the strings
bool StringEqualsCaseInsen(std::string_view lhs, std::string_view rhs);
bool StringEqualsCaseInsen(std::string_view lhs, std::wstring_view rhs);

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Fuzzy Matching
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct FuzzyMatchResult {
	u64 position = 0u;
	u64 length = 0u;
	u64 matchedCount = 0u;
};

// fuzzy matches a string
// @TODO doucument this function
bool FuzzyMatch(std::string_view pattern, std::string_view fullText, /*out*/ FuzzyMatchResult* result);

int CompareFuzzyMatchResults(const FuzzyMatchResult& lhs, const FuzzyMatchResult& rhs);

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// File and Path helpers
//
///////////////////////////////////////////////////////////////////////////////////////////////////

struct _FILETIME;

// read an entire file
// optionally also gets the last write time of the file
bool ReadEntireFile(std::string_view path, /*out*/ std::string* buffer, /*out*/ _FILETIME* lastWriteTime = nullptr);

// takes a path and creates the coresponding file:///-Uri
std::string MakeUriFromPath(std::string_view path);

// takes a file:///-Uri and extracts the filepath from it, decodes special characeter (e.g. %20 = space)
std::string MakePathFromUri(std::string_view uri);

// takes a path and returns the directory/filename/extension
std::string_view GetFilenameFromPath(std::string_view path);
std::string_view GetExtensionFromPath(std::string_view path);
std::string_view GetDirectoryFromPath(std::string_view path);

// get the current working directory/path in which the current .exe resides
// returned strings do have a null-terminator
// WARNING! uses a static buffer to hold the data!
std::string_view GetProcessDirectory();
std::string_view GetWorkingDirectory();

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Directory iterator
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// iterate over all files and sub-directories in a dir
struct DirectoryIterator {

	//-----------------------------------------------------
	// data
	//-----------------------------------------------------

	void* hHandle = nullptr;
	u64 fileSize = 0u;
	u32 fileAttributes = 0u;
	u32 lastError = 0u;
	std::string filename = {};
	char searchPath [260] {0}; // MAX_PATH

	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------

	bool Next();
	bool Failed() const;

	bool IsDirectory() const;
	bool IsFile() const;
	std::string_view GetSearchPath() const;

	DirectoryIterator() noexcept;
	explicit DirectoryIterator(std::string_view directoryPath) noexcept;
	~DirectoryIterator() noexcept;
};