#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MissionScenarioDemoPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USpringArmComponent;
class UStaticMeshComponent;
class AMissionScenarioDemoActor;

UCLASS(BlueprintType, Blueprintable)
class MISSIONSCENARIORUNTIME_API AMissionScenarioDemoPawn : public APawn
{
	GENERATED_BODY()

public:
	AMissionScenarioDemoPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Playable Demo")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Playable Demo")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Playable Demo")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission Scenario Runtime|Playable Demo")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Scenario Runtime|Playable Demo")
	float TurnRate = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Scenario Runtime|Playable Demo")
	float LookUpRate = 35.0f;

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	void MoveForward(float Value);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	void MoveRight(float Value);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	void Turn(float Value);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	void LookUp(float Value);

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	bool AdvanceScenarioObjective();

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	bool InteractWithCurrentObjective();

	UFUNCTION(BlueprintCallable, Category = "Mission Scenario Runtime|Playable Demo")
	bool EvaluateObjectiveProgress(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Mission Scenario Runtime|Playable Demo")
	FString GetLastScenarioInteractionMessage() const;

private:
	AMissionScenarioDemoActor* FindDemoScenarioActor() const;
	AActor* FindObjectiveMarker(const FString& ObjectiveId) const;
	bool IsNearObjectiveMarker(const FString& ObjectiveId, float AcceptanceRadius) const;
	bool TryCompleteActiveObjective(const FString& CompletionMessage);
	void HandleAdvanceScenarioObjectiveInput();

	float HoldObjectiveElapsedSeconds = 0.0f;
	FString LastScenarioInteractionMessage;
};
