#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionScenarioInstance.h"
#include "MissionScenarioDemoActor.generated.h"

class USceneComponent;
class UTextRenderComponent;

UCLASS(BlueprintType, Blueprintable)
class MISSIONSCENARIORUNTIME_API AMissionScenarioDemoActor : public AActor
{
	GENERATED_BODY()

public:
	AMissionScenarioDemoActor();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Scenario Runtime|Demo", meta = (MultiLine = "true"))
	FString ScenarioJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Scenario Runtime|Demo")
	bool bInitializeOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Scenario Runtime|Demo")
	bool bStartOnBeginPlay = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Demo")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Demo")
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Mission Scenario Runtime|Demo")
	TObjectPtr<UMissionScenarioInstance> ScenarioInstance;

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	bool InitializeDemoScenario(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	bool StartDemoScenario(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	bool AdvanceDemoObjective(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	bool RunDemoScenarioToCompletion(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	void FailDemoScenario(const FString& Reason);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	void ResetDemoScenario();

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	void SetDemoStatusMessage(const FString& Message);

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Demo")
	FString GetDemoStatusMessage() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Demo")
	FMissionScenarioRuntimeSnapshot GetDemoSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Demo")
	TArray<FMissionScenarioEventRecord> GetDemoEventLog() const;

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Demo")
	FString ExportDemoEventLogToJson() const;

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Demo")
	UMissionScenarioInstance* GetScenarioInstance() const;

private:
	UPROPERTY(Transient)
	FString LastStatusMessage;

	UMissionScenarioInstance* EnsureScenarioInstance();
	FString BuildObjectiveDisplayText() const;
	void ApplyStatusTextPresentation() const;
	void UpdateStatusText();
};
