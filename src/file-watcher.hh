#pragma once
#include "basic.hh"
#include <vector>
#include <string_view>

struct WatchJob;

struct FileWatcher {
	
	//-------------------------------------------
	// types
	
	// should match the constants in windows.h
	enum Action {
		 Action_None,
		 Action_Added,
		 Action_Removed,
		 Action_Modified,
		 Action_RenamedOldName,
		 Action_RenamedNewName
	};
		
	using OnChangeHandler = void (*) (void* userdata, Action action, std::string_view fileName);

	//-------------------------------------------
	// data
	
	std::vector<WatchJob*> watchJobs = {};
	
	void* hThread = nullptr;
	void* hEventExitThread = nullptr;
	
	u8 buffer[1024];
	
	//-------------------------------------------
	// functions
	
	bool Init();
	void Shutdown();
	
	bool WatchDirectory(std::string_view path, u32 notifyFilter, OnChangeHandler OnChange, void* userdata = nullptr);
};

extern FileWatcher fileWatcher;
