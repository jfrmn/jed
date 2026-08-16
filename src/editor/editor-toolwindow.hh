#pragma once
#include "basic.hh"

struct Command;
struct KeyEvent;
struct EditorSearch;
struct EditorGotoLine;
struct EditorDiagnosticsList;

// The Toolwindow is on the top right corner of the editor
// It can be the Search&Replace, the Goto Line-Window or the Diagnostics-List
struct EditorToolWindow {
	virtual ~EditorToolWindow();
	
	virtual bool IsSearch() const;
	virtual bool IsGotoLine() const;
	virtual bool IsDiagnosticsList() const;
	
	virtual void OnUpdate() = 0;
	virtual bool OnKeyEvent(KeyEvent event, Command command);
	virtual void OnChar(const char* data, u64 len);
};
