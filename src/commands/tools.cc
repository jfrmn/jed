#include "tools.hh"
#include "logging.hh"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d2d1.h>

#define TOML_ABI_NAMESPACES 0
#define TOML_ENABLE_UNRELEASED_FEATURES 1
#define TOML_EXCEPTIONS 0
#define TOML_IMPLEMENTATION 0
#include <toml++/toml.hpp>


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
std::vector<Tool> Tool::tools {};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static void ReadRegexCaptureGroup(const toml::table* table, const Regex& regex, std::string_view key, /*out*/ u32* captureGroup) {
	auto valGroup = table->get_as<s64>(key);
	if (!valGroup) return;
	if (valGroup->get() >= regex.captureGroupCount + 1u || valGroup->get() < 0u) {
		LogWarning("%s: value is out of range. regex only provides %u groups", Str(valGroup->source()), regex.captureGroupCount + 1u);
		return;
	}
			
	*captureGroup = static_cast<u32>(valGroup->get());
}

bool Tool::LoadTools(toml::node* toml) {
	
	toml::array* array = toml->as_array();
	if (!array) {
		LogError("%s: expected an array", Str(toml->source()));
		return false;
	}
	
	Tool tool {};
	for (toml::node& node : *array) {
		
		toml::table* tblTool = node.as_table();
		if (!tblTool) {
			LogError("%s: expected a table", Str(node.source()));
			continue;
		}
		
		//
		// name 
		//
		auto valName = tblTool->get_as<std::string>("name");
		if (!valName) {
			LogError("%s: expected entry 'name' as string", Str(tblTool->source()));
			continue;
		}
		tool.name = std::move(valName->get());
		
		//
		// command 
		//
		auto valCommand = tblTool->get_as<std::string>("command");
		if (!valCommand) {
			LogError("%s: expected entry 'command' as string", Str(tblTool->source()));
			continue;	
		}
		tool.command = std::move(valCommand->get());
		
		//
		// force-configuration 
		//
		if (auto valForceConfig = tblTool->get_as<bool>("force-configuration"))
			tool.forceConfiguration = valForceConfig->get();
		
		//
		// external
		//
		if (auto valExternal = tblTool->get_as<bool>("external"))
			tool.external = valExternal->get();
			
		//
		// enviornment 
		//
		if (auto nodeEnv = tblTool->get_as<toml::table>("environment")) {
			for (toml::impl::table_proxy_pair<false> kvp : *nodeEnv) {
				auto valValue = kvp.second.as_string();
				if (!valValue) continue;
			
				tool.environment.append(kvp.first);
				tool.environment.push_back('=');
				tool.environment.append(std::move(valValue->get()));
				tool.environment.push_back('\0');
			}
			tool.environment.push_back('\0');
		}
		
		//
		// parameter 
		//
		if (auto arrParameters = tblTool->get_as<toml::array>("parameters")) {
			tool.parameters.reserve(arrParameters->size());
			
			for (u64 i = 0u; i < arrParameters->size(); i++) {
				toml::node* node = arrParameters->get(i);
				ASSERT(node);
				
				Parameter parameter {};
				if (!Parameter::FromToml(*node, &parameter))
					continue;
			
				tool.parameters.push_back(std::move(parameter));
			}
		}
		
		//
		// open-console
		//
		if (auto valOpenFlags = tblTool->get_as<std::string>("open-console")) {
			if      (valOpenFlags->get() == "never")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_Never;
			else if (valOpenFlags->get() == "on-start")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_OnStart;
			else if (valOpenFlags->get() == "on-exit-success")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_OnExitSuccess;
			else if (valOpenFlags->get() == "on-exit-error")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_OnExitSuccess;
			else if (valOpenFlags->get() == "on-exit")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_OnExit;
			else if (valOpenFlags->get() == "always")
				tool.consoleOpenFlags = ConsoleOpenFlags::ConsoleOpenFlags_Always;
			else
				LogWarning("%s: unknown open-console value '%.*s'", Str(valOpenFlags->source()), (int)valOpenFlags->get().size(), valOpenFlags->get().data());
		}
		
		//
		// progress
		//
		if (auto tblProgress = tblTool->get_as<toml::table>("progress")) {
			
			if (auto valRegex = tblProgress->get_as<std::string>("regex")) {
				RegexError err;
				const bool ok = tool.progress.regex.Compile(valRegex->get(), &err);
				if (!ok) LogError("%s: regex did not compile: %s", Str(valRegex->source()), err.message.c_str());
				
			} else {
				LogWarning("%s: expected entry 'regex' as string", Str(valRegex->source()));
			}
			
			ReadRegexCaptureGroup(tblProgress, tool.progress.regex, "group-value", &tool.progress.captureGroupValue);
			ReadRegexCaptureGroup(tblProgress, tool.progress.regex, "group-max", &tool.progress.captureGroupMax);
			
			if (auto valMax = tblProgress->get_as<s64>("max-value"))
				tool.progress.maxValue = valMax->get();
						
			if (auto valFormat = tblProgress->get_as<std::string>("format")) {
				if (valFormat->get() == "none")
					tool.progress.format = Progress::Format_None;
				else if (valFormat->get() == "percent")
					tool.progress.format = Progress::Format_Percent;
				else if (valFormat->get() == "absolute")
					tool.progress.format = Progress::Format_Absolute;
				else
					LogWarning("%s: unknown format value: '%.*s'", Str(valFormat->source()), (int)valFormat->get().size(), valFormat->get().data());
			}
			
			if (auto valHideFromStatusBar = tblProgress->get_as<s64>("hide-from-status-bar"))
				tool.progress.hideFromStatusBar = valHideFromStatusBar->get();
		}
		
		//
		// diagnostics 
		//
		if (auto tblDiagnostics = tblTool->get_as<toml::table>("diagnostics")) {
			
			if (auto valRegex = tblDiagnostics->get_as<std::string>("regex")) {
				RegexError err;
				const bool ok = tool.diagnostics.regex.Compile(valRegex->get(), &err);
				if (!ok) LogWarning("%s: regex did not compile: %s", Str(valRegex->source()), err.message.c_str());
			}
			
			ReadRegexCaptureGroup(tblDiagnostics, tool.diagnostics.regex, "group-file", &tool.diagnostics.captureGroupFile);			
			ReadRegexCaptureGroup(tblDiagnostics, tool.diagnostics.regex, "group-line", &tool.diagnostics.captureGroupLine);			
			ReadRegexCaptureGroup(tblDiagnostics, tool.diagnostics.regex, "group-color", &tool.diagnostics.captureGroupColor);			
			
			if (auto valLinesStartAtOne = tblDiagnostics->get_as<bool>("lines-start-at-one"))
				tool.diagnostics.linesStartAtOne = valLinesStartAtOne->get();
				
			if (auto tblColorMapping = tblDiagnostics->get_as<toml::table>("color-mapping")) {
				
				tool.diagnostics.colorMapping.reserve(tblColorMapping->size());
				for (toml::impl::table_proxy_pair<false> kvp : *tblColorMapping) {
					Color color;
					if (!Color::FromToml(kvp.second, &color))
						continue;
				
					tool.diagnostics.colorMapping.push_back(DiagnosticsMatcher::ColorMapping {
						.key = std::string {kvp.first.str()},
						.color = color});
				}
			}
		}
		
		tools.push_back(std::move(tool));
	}
	
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Tool::GetDefaultValues(/*out*/ std::vector<ParameterValue>* parameterValues) const {
	parameterValues->clear();
	parameterValues->reserve(parameters.size());
	for (u64 i = 0u; i < parameters.size(); i++)
		parameterValues->push_back(parameters[i].defaultValue);
}
