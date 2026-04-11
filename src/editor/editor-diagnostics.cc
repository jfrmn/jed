#include "editor-diagnostics.hh"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void EditorDiagnostics::Reset() {
	std::scoped_lock lock {mutex};
	records.clear();
	diagnosticsVersion = 0u;
}

u64 EditorDiagnostics::RecordCount() const {
	return records.size();
}


bool EditorDiagnostics::IsEmpty() const {
	return records.empty();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorDiagnostics::Record& EditorDiagnostics::operator[](u64 i) { return records[i]; }
const EditorDiagnostics::Record& EditorDiagnostics::operator[](u64 i) const { return records[i]; }