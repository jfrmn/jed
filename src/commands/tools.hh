#pragma once
#include "commands/parameter.hh"

#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2dbasetypes.h>

struct cJSON;

struct Tool {
	
	//-----------------------------------------------------
	// statics
	//-----------------------------------------------------
	
	static std::vector<Tool> tools;
	
	static bool LoadTools(const cJSON* json);
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
		
	struct Progress {
		enum Format {
			 Format_None = 0,
			 Format_Percent,
			 Format_Absolute,
		};
		
		Format format = Format_Percent;
		std::string regex = {};
		u64 captureGroupValue = 0;
		u64 captureGroupMax = U64_MAX;
		s64 maxValue = 100;
		bool hideFromStatusBar = false;
	};
	
	enum ConsoleOpenFlags {
	     ConsoleOpenFlags_Never          = 0,
	     ConsoleOpenFlags_OnStart        = 1 << 0,
	     ConsoleOpenFlags_OnExitSuccess  = 1 << 1,
	     ConsoleOpenFlags_OnExitError    = 1 << 2,
	     ConsoleOpenFlags_OnExit         = ConsoleOpenFlags_OnExitSuccess | ConsoleOpenFlags_OnExitError,
	     ConsoleOpenFlags_Always         = ConsoleOpenFlags_OnStart | ConsoleOpenFlags_OnExit
	};
	
	struct DiagnosticsMatcher {
		
		struct ColorMapping {
			std::string key = {};
			D2D_COLOR_F color = {};
		};
		
		std::string regex = {};
		u64 captureGroupFile = U64_MAX;
		u64 captureGroupLine = U64_MAX;
		u64 captureGroupColor = U64_MAX;
		
		bool linesStartAtOne = false;
		
		std::vector<ColorMapping> colorMapping = {};		
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	std::string name = {};
	std::string description = {};
	std::string command = {};
	int flags = 0;
	
	std::vector<Parameter> parameters = {};
	std::string environment = {};
	
	ConsoleOpenFlags consoleOpenFlags = ConsoleOpenFlags_Never;
	Progress progress = {};
	DiagnosticsMatcher diagnosticsMatcher = {};	
	
	bool forceConfiguration = false;	
	bool external = false; // @TODO not implemented
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	void GetDefaultValues(/*out*/ std::vector<ParameterValue>* parameterValues) const;
	bool HasProgress() const;
	bool HasDiagnosticsMatcher() const;
};
