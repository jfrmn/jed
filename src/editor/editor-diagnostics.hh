#pragma once
#include "basic.hh"
#include "text/text-position.hh"
#include "util/diagnostics.hh"

#include <string>
#include <vector>
#include <mutex>

struct EditorDiagnostics {

	//-------------------------------------------
	// types
	//-------------------------------------------

	struct Record {
		TextPosition from = {};
		TextPosition to   = {};
		
		std::string code = {};
		std::string message = {};

		Diagnostics::Severity severity = Diagnostics::Severity_Unknown;
	};
	
	//-------------------------------------------
	// data
	//-------------------------------------------
	
	std::mutex mutex = {};
	
	std::vector<Record> records = {};
	
	s32 diagnosticsVersion = 0;
	
	//-------------------------------------------
	// functions
	//-------------------------------------------
	
	void Reset();
	u64 RecordCount() const;
	bool IsEmpty() const;
	
	std::vector<Record>::iterator begin();
	std::vector<Record>::const_iterator begin() const;
	std::vector<Record>::iterator end();
	std::vector<Record>::const_iterator end() const;
};