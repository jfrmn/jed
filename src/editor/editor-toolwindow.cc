#include "editor-toolwindow.hh"
#include "events.hh"

bool EditorToolWindow::IsSearch() const {
	return false;
}
bool EditorToolWindow::IsGotoLine() const {
	return false;
}
bool EditorToolWindow::IsDiagnosticsList() const {
	return false;
}

bool EditorToolWindow::OnKeyDown(KeyEvent) {
	return false;
}

bool EditorToolWindow::OnChar(const char* data, u64 len) {
	return false;
}

EditorToolWindow::~EditorToolWindow() {
}
