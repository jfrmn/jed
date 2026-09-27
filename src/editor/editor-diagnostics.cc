#include "editor-diagnostics.hh"
#include <algorithm>

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

void EditorDiagnostics::Sort() {
	std::sort(records.begin(), records.end(), [] (const Record& lhs, const Record& rhs) {
		return (lhs.from < rhs.from);
	});
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
EditorDiagnostics::Record& EditorDiagnostics::operator[](u64 i) { return records[i]; }
const EditorDiagnostics::Record& EditorDiagnostics::operator[](u64 i) const { return records[i]; }