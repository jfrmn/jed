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

bool EditorToolWindow::HandleEvent(const Event&, const Command&) {
	return false;
}

EditorToolWindow::~EditorToolWindow() {
}
