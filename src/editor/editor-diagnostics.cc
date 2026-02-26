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
std::vector<EditorDiagnostics::Record>::iterator EditorDiagnostics::begin() { return records.begin(); }
std::vector<EditorDiagnostics::Record>::const_iterator EditorDiagnostics::begin() const { return records.begin(); }
std::vector<EditorDiagnostics::Record>::iterator EditorDiagnostics::end() { return records.end(); }
std::vector<EditorDiagnostics::Record>::const_iterator EditorDiagnostics::end() const { return records.end(); }

