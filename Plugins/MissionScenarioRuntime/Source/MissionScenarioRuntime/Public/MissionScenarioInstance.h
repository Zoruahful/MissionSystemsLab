#pragma once

#include "CoreMinimal.h"
#include "MissionScenarioTypes.h"
#include "UObject/Object.h"
#include "MissionScenarioInstance.generated.h"

UENUM(BlueprintType)
enum class EMissionScenarioRunState : uint8
{
	Uninitialized UMETA(DisplayName = "Uninitialized"),
	Ready UMETA(DisplayName = "Ready"),
	Running UMETA(DisplayName = "Running"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EMissionScenarioEventType : uint8
{
	Initialized UMETA(DisplayName = "Initialized"),
	Started UMETA(DisplayName = "Started"),
	ObjectiveCompleted UMETA(DisplayName = "Objective Completed"),
	ObjectiveAdvanced UMETA(DisplayName = "Objective Advanced"),
	Completed UMETA(DisplayName = "Completed"),
	Failed UMETA(DisplayName = "Failed"),
	Reset UMETA(DisplayName = "Reset"),
	InvalidAction UMETA(DisplayName = "Invalid Action")
};

USTRUCT(BlueprintType)
struct MISSIONSCENARIORUNTIME_API FMissionScenarioRuntimeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	EMissionScenarioRunState RunState = EMissionScenarioRunState::Uninitialized;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 CurrentPhaseIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 CurrentObjectiveIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString CurrentPhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString CurrentObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 CompletedObjectiveCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 TotalObjectiveCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString FailureReason;
};

USTRUCT(BlueprintType)
struct MISSIONSCENARIORUNTIME_API FMissionScenarioEventRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 EventIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString TimestampUtc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString ScenarioId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	EMissionScenarioEventType EventType = EMissionScenarioEventType::InvalidAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	EMissionScenarioRunState RunState = EMissionScenarioRunState::Uninitialized;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString PhaseId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 CompletedObjectiveCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	int32 TotalObjectiveCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission Scenario")
	FString Message;
};

UCLASS(BlueprintType)
class MISSIONSCENARIORUNTIME_API UMissionScenarioInstance : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime")
	bool InitializeFromDefinition(const FMissionScenarioDefinition& InScenarioDefinition, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime", meta = (DisplayName = "Initialize Mission Scenario From JSON"))
	bool InitializeFromJson(const FString& ScenarioJson, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime")
	bool StartScenario(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime")
	bool CompleteCurrentObjective(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime")
	void FailScenario(const FString& Reason);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Runtime")
	void ResetScenario();

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	EMissionScenarioRunState GetRunState() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool IsRunning() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool IsScenarioComplete() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool IsScenarioFailed() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	FMissionScenarioDefinition GetScenarioDefinition() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool GetCurrentPhase(FMissionScenarioPhaseDefinition& OutPhase) const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	bool GetCurrentObjective(FMissionScenarioObjectiveDefinition& OutObjective) const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Runtime")
	FMissionScenarioRuntimeSnapshot GetSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Event Log")
	TArray<FMissionScenarioEventRecord> GetEventLog() const;

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Event Log")
	FString ExportEventLogToJson() const;

private:
	UPROPERTY()
	FMissionScenarioDefinition ScenarioDefinition;

	UPROPERTY()
	EMissionScenarioRunState RunState = EMissionScenarioRunState::Uninitialized;

	UPROPERTY()
	int32 CurrentPhaseIndex = INDEX_NONE;

	UPROPERTY()
	int32 CurrentObjectiveIndex = INDEX_NONE;

	UPROPERTY()
	int32 CompletedObjectiveCount = 0;

	UPROPERTY()
	FString FailureReason;

	UPROPERTY()
	TArray<FMissionScenarioEventRecord> EventLog;

	bool HasCurrentObjective() const;
	int32 GetTotalObjectiveCount() const;
	void AdvanceToNextObjectiveOrComplete();
	void AppendEvent(
		EMissionScenarioEventType EventType,
		const FString& Message,
		const FString& PhaseIdOverride = FString(),
		const FString& ObjectiveIdOverride = FString());
};
