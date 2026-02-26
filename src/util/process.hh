#pragma once
#include "basic.hh"

#include <string>

struct Logger;

struct Process {

	//-----------------------------------------------------
	// types

	struct Observer {

		virtual void OnStderr(std::string_view data) = 0;
		virtual void OnStdout(std::string_view data) = 0;
		
		virtual void OnStarted() {};
		virtual void OnExited(int exitCode) {};
	};

	using Handle = void*;

	struct ThreadData {
		Handle hPipe = NULL;
		Process* process = nullptr;
	};
	
	struct StartInfo {
		std::string_view application = {};
		std::string commandLine = {};
		std::string_view environment = {};
		int flags = 0; // @TODO make sure that CREATE_PRESERVE_CODE_AUTHZ_LEVEL is never passed because that sounds hella scetchy
		
	};

	//-----------------------------------------------------
	// data

	Handle hProcess = nullptr;
	Handle hProcessMainThread = nullptr;

	Handle hThreadReadStdout = nullptr;
	Handle hThreadReadStderr = nullptr;

	Handle hPipeStdin = nullptr;

	ThreadData threadDataStdout = {};
	ThreadData threadDataStderr = {};

	Observer* observer = {};

	//-----------------------------------------------------
	// functions

	static bool StartDetached(StartInfo startInfo);

	bool Start(StartInfo startInfo);
	
	u32 GetExitCode() const;

	bool IsRunning() const;
	bool Terminate(u32 exitCode = U32_MAX);

	bool WriteToStdin(std::string_view data);

	DISALLOW_COPY_AND_ASSING(Process);
	Process(Process &&other) noexcept;
};
