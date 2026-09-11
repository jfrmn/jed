#include "checks.hh"
#include "tools.hh"
#include "app.hh"

static bool WaitForProcessExit() {
	// There are certainly better ways to do this, than a spinlock
	// but this shouldn't take too long, so it's fine.
	
	for (u64 i = 0; i < 10; i++) {
		if (!app.toolOutput.process->IsRunning())
			return true;
		SleepEx(100, FALSE);
	}
	return false;
}

void Test_ToolOutput_RunTestTool() {
	
	// we could also hard code the tool in here
	// instead of relying on it being in the settings
	
	const Tool* tool = nullptr;
	for (const Tool& t : Tool::tools) {
		if (t.name == "test tool") {
			tool = &t;
			break;
		}
	}
	
	REQUIRE_NOT_NULL(tool);
	
	app.toolOutput.tool = tool;
	app.toolOutput.toolParameterValues.resize(1u);
	app.toolOutput.toolParameterValues.front() = ParameterValue {.boolValue = true}; // skip delays
	
	PushEvent(Event {
		.type = Event::Type_Command,
		.cmd = Command {
			.id = Command::Id_ToggleToolOutput}});
	CHECK_TRUE(app.toolOutput.isOpen);
	DEFER({
		PushEvent(Event {
		.type = Event::Type_Command,
		.cmd = Command {
			.id = Command::Id_ToggleToolOutput}});
		CHECK_FALSE(app.toolOutput.isOpen);
	});
	
	CHECK_TRUE(app.toolOutput.StartProcess());
	REQUIRE_NOT_NULL(app.toolOutput.process);
	
	Update();
	
	REQUIRE_TRUE(WaitForProcessExit());
	CHECK_EQ(app.toolOutput.diagnosticsRecords.size(), 14);
	CHECK_EQ(app.toolOutput.toolDiagnostics.size(), 1);
	
	PushEvent(Event {
		.type = Event::Type_Command,
		.cmd = Command {
			.id = Command::Id_GotoNextDiagnosticRecord}});
	
	CHECK_NEQ(app.toolOutput.selectionStart, app.toolOutput.selectionEnd);
	CHECK_FALSE(app.toolOutput.filePreview.hasError);
	CHECK_TRUE(app.toolOutput.filePreview.hasSelection);
	CHECK_FALSE(app.toolOutput.filePreview.lines.empty());
	
	/* Not implemented yet
	const Process* oldProcess = app.toolOutput.process;
	const f32 clickX = 540.0f;
	const f32 clickY = 30.0f;
	
	PushEvent(Event {
		.type = Event::Type_MouseDown,
		.mouse = {
			.x = clickX,
			.y = clickY}});
	PushEvent(Event {
		.type = Event::Type_MouseUp,
		.mouse = {
			.x = clickX,
			.y = clickY}});
	CHECK_NEQ(app.toolOutput.process, oldProcess);
	
	REQUIRE_TRUE(WaitForProcessExit());
	*/
}
