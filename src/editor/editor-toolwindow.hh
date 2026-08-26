#pragma once

struct Command;
struct Event;

// The Toolwindow is on the top right corner of the editor
// It can be the Search&Replace, the Goto Line-Window or the Diagnostics-List
struct EditorToolWindow {
	virtual ~EditorToolWindow();
	
	virtual bool IsSearch() const;
	virtual bool IsGotoLine() const;
	virtual bool IsDiagnosticsList() const;
	
	virtual void Update() = 0;
	virtual bool HandleEvent(const Event& event, const Command& command);
};
