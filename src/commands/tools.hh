#pragma once
#include "commands/parameter.hh"
#include "util/color.hh"
#include "util/regex.hh"

#include <string>
#include <vector>

struct cJSON;
namespace toml { class node; }

struct Tool {
	
	//-----------------------------------------------------
	// statics
	//-----------------------------------------------------
	
	static std::vector<Tool> tools;
	
	static bool LoadTools(toml::node* toml);
	
	//-----------------------------------------------------
	// types
	//-----------------------------------------------------
		
	struct Progress {
		enum Format {
			 Format_None = 0,
			 Format_Percent,
			 Format_Absolute,
		};
		
		Regex regex = {};
		u32 captureGroupValue = 0;
		u32 captureGroupMax = U32_MAX;
		s64 maxValue = 100;
		
		Format format = Format_Percent;
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
			Color color = {};
		};
		
		Regex regex = {};
		u32 captureGroupFile  = U32_MAX;
		u32 captureGroupLine  = U32_MAX;
		u32 captureGroupColor = U32_MAX;
		
		bool linesStartAtOne = false;
		
		std::vector<ColorMapping> colorMapping = {};		
	};
	
	//-----------------------------------------------------
	// data
	//-----------------------------------------------------
	
	std::string name = {};
	std::string command = {};
	std::string environment = {};
	int flags = 0;
	
	std::vector<Parameter> parameters = {};
	
	ConsoleOpenFlags consoleOpenFlags = ConsoleOpenFlags_Never;
	Progress progress = {};
	DiagnosticsMatcher diagnostics = {};	
	
	bool forceConfiguration = false;	
	bool external = false; // @TODO not implemented
	
	//-----------------------------------------------------
	// functions
	//-----------------------------------------------------
	
	void GetDefaultValues(/*out*/ std::vector<ParameterValue>* parameterValues) const;
};
