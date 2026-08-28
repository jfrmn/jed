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

bool EditorToolWindow::HandleEvent(const Event&) {
	return false;
}

EditorToolWindow::~EditorToolWindow() {
}
