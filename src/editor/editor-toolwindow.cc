#include "editor-toolwindow.hh"
#include "events.hh"
#include "commands.hh"

bool EditorToolWindow::IsSearch() const {
	return false;
}
bool EditorToolWindow::IsGotoLine() const {
	return false;
}
bool EditorToolWindow::IsDiagnosticsList() const {
	return false;
}

bool EditorToolWindow::OnKeyEvent(KeyEvent, Command) {
	return false;
}

void EditorToolWindow::OnChar(const char* data, u64 len) {
}

EditorToolWindow::~EditorToolWindow() {
}
