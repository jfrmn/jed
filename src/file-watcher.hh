#pragma once
#include "basic.hh"
#include <vector>
#include <string_view>

struct WatchedDirectory;

struct FileWatcher {
	
	//-------------------------------------------
	// data
	
	std::vector<WatchedDirectory*> watchedDirectories = {};
	
	void* hThread = nullptr;
	void* hEventExitThread = nullptr;
	
	u8 buffer[1024];
	
	//-------------------------------------------
	// functions
	
	bool Init();
	void Shutdown();
	
	bool SubscribeDirectoryOfFile(std::string_view filepath);
	bool UnsubscribeDirectoryOfFile(std::string_view filepath);
};

extern FileWatcher fileWatcher;
