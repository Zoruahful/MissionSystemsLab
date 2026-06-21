#include "MissionScenarioRuntime.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMissionScenarioRuntime);

class FMissionScenarioRuntimeModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogMissionScenarioRuntime, Log, TEXT("MissionScenarioRuntime module started."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogMissionScenarioRuntime, Log, TEXT("MissionScenarioRuntime module shut down."));
	}
};

IMPLEMENT_MODULE(FMissionScenarioRuntimeModule, MissionScenarioRuntime)
