#pragma once

#include "CoreMinimal.h"
#include "MissionScenarioTypes.generated.h"

UENUM(BlueprintType)
enum class EMissionScenarioObjectiveType : uint8
{
	ReachLocation UMETA(DisplayName = "Reach Location"),
	Interact UMETA(DisplayName = "Interact"),
	Wait UMETA(DisplayName = "Wait"),
	Custom UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct MISSIONSCENARIORUNTIME_API FMissionScenarioObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	EMissionScenarioObjectiveType ObjectiveType = EMissionScenarioObjectiveType::Custom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString TargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario", meta = (ClampMin = "0.0", Units = "s"))
	float TimeLimitSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct MISSIONSCENARIORUNTIME_API FMissionScenarioPhaseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString PhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	TArray<FMissionScenarioObjectiveDefinition> Objectives;
};

USTRUCT(BlueprintType)
struct MISSIONSCENARIORUNTIME_API FMissionScenarioDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString SchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario", meta = (MultiLine = "true"))
	FString Briefing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	TArray<FMissionScenarioPhaseDefinition> Phases;
};
