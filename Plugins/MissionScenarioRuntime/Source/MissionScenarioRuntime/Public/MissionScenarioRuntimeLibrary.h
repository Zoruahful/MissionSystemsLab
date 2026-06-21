#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MissionScenarioInstance.h"
#include "MissionScenarioTypes.h"
#include "MissionScenarioRuntimeLibrary.generated.h"

UCLASS()
class MISSIONSCENARIORUNTIME_API UMissionScenarioRuntimeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime")
	static FString GetMissionScenarioRuntimeVersion();

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime")
	static bool IsMissionScenarioRuntimeAvailable();

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Scenario")
	static FString GetScenarioContractVersion();

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Scenario", meta = (DisplayName = "Parse Mission Scenario JSON"))
	static bool ParseScenarioJson(const FString& ScenarioJson, FMissionScenarioDefinition& OutScenario, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Scenario")
	static FString GetScenarioJsonValidationError(const FString& ScenarioJson);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime", meta = (WorldContext = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static UMissionScenarioInstance* CreateMissionScenarioInstance(UObject* WorldContextObject);
};
