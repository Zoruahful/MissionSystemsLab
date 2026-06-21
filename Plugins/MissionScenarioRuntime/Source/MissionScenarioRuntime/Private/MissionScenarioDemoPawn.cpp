#include "MissionScenarioDemoPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MissionScenarioDemoActor.h"
#include "MissionScenarioInstance.h"
#include "MissionScenarioRuntime.h"
#include "MissionScenarioTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr float ReachObjectiveRadius = 150.0f;
constexpr float HoldObjectiveRadius = 170.0f;
constexpr float ConsoleInteractionRadius = 160.0f;
constexpr float DemoHoldRequiredSeconds = 5.0f;

FName GetMarkerLabelForObjective(const FString& ObjectiveId)
{
	if (ObjectiveId == TEXT("reach_alpha"))
	{
		return TEXT("P1_ObjectiveMarker_Alpha");
	}

	if (ObjectiveId == TEXT("hold_position"))
	{
		return TEXT("P1_ObjectiveMarker_Hold");
	}

	if (ObjectiveId == TEXT("interact_console"))
	{
		return TEXT("P1_ObjectiveMarker_Console");
	}

	return NAME_None;
}
}

AMissionScenarioDemoPawn::AMissionScenarioDemoPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetCollisionProfileName(TEXT("Pawn"));
	BodyMesh->SetWorldScale3D(FVector(0.65f, 0.65f, 0.65f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 420.0f;
	CameraBoom->SetRelativeRotation(FRotator(-20.0f, 0.0f, 0.0f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 70.0f;

	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("MovementComponent"));
	MovementComponent->MaxSpeed = 650.0f;
	MovementComponent->Acceleration = 2400.0f;
	MovementComponent->Deceleration = 2800.0f;
}

void AMissionScenarioDemoPawn::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EvaluateObjectiveProgress(DeltaSeconds);
}

void AMissionScenarioDemoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (PlayerInputComponent == nullptr)
	{
		return;
	}

	PlayerInputComponent->BindAxis(TEXT("P1_MoveForward"), this, &AMissionScenarioDemoPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("P1_MoveRight"), this, &AMissionScenarioDemoPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("P1_Turn"), this, &AMissionScenarioDemoPawn::Turn);
	PlayerInputComponent->BindAxis(TEXT("P1_LookUp"), this, &AMissionScenarioDemoPawn::LookUp);
	PlayerInputComponent->BindAction(TEXT("P1_AdvanceObjective"), IE_Pressed, this, &AMissionScenarioDemoPawn::HandleAdvanceScenarioObjectiveInput);
}

UPawnMovementComponent* AMissionScenarioDemoPawn::GetMovementComponent() const
{
	return MovementComponent;
}

void AMissionScenarioDemoPawn::MoveForward(const float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AMissionScenarioDemoPawn::MoveRight(const float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AMissionScenarioDemoPawn::Turn(const float Value)
{
	const UWorld* World = GetWorld();
	if (!FMath::IsNearlyZero(Value) && World != nullptr)
	{
		AddControllerYawInput(Value * TurnRate * World->GetDeltaSeconds());
	}
}

void AMissionScenarioDemoPawn::LookUp(const float Value)
{
	const UWorld* World = GetWorld();
	if (!FMath::IsNearlyZero(Value) && World != nullptr)
	{
		AddControllerPitchInput(Value * LookUpRate * World->GetDeltaSeconds());
	}
}

bool AMissionScenarioDemoPawn::AdvanceScenarioObjective()
{
	AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		LastScenarioInteractionMessage = TEXT("No mission scenario demo actor was found in the current world.");
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	FString Error;
	const bool bAdvanced = DemoActor->AdvanceDemoObjective(Error);
	if (bAdvanced)
	{
		const FMissionScenarioRuntimeSnapshot Snapshot = DemoActor->GetDemoSnapshot();
		LastScenarioInteractionMessage = FString::Printf(
			TEXT("Advanced scenario objective. Progress %d/%d."),
			Snapshot.CompletedObjectiveCount,
			Snapshot.TotalObjectiveCount);
		UE_LOG(LogMissionScenarioRuntime, Log, TEXT("%s"), *LastScenarioInteractionMessage);
		return true;
	}

	LastScenarioInteractionMessage = Error.IsEmpty()
		? TEXT("Scenario objective could not be advanced.")
		: Error;
	UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
	return false;
}

bool AMissionScenarioDemoPawn::EvaluateObjectiveProgress(const float DeltaSeconds)
{
	AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		return false;
	}

	UMissionScenarioInstance* ScenarioInstance = DemoActor->GetScenarioInstance();
	if (ScenarioInstance == nullptr || !ScenarioInstance->IsRunning())
	{
		HoldObjectiveElapsedSeconds = 0.0f;
		return false;
	}

	FMissionScenarioObjectiveDefinition Objective;
	if (!ScenarioInstance->GetCurrentObjective(Objective))
	{
		HoldObjectiveElapsedSeconds = 0.0f;
		return false;
	}

	if (Objective.ObjectiveId == TEXT("reach_alpha"))
	{
		HoldObjectiveElapsedSeconds = 0.0f;
		if (IsNearObjectiveMarker(Objective.ObjectiveId, ReachObjectiveRadius))
		{
			return TryCompleteActiveObjective(TEXT("Reached Alpha."));
		}

		LastScenarioInteractionMessage = TEXT("Move to ALPHA.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		return false;
	}

	if (Objective.ObjectiveId == TEXT("hold_position"))
	{
		if (!IsNearObjectiveMarker(Objective.ObjectiveId, HoldObjectiveRadius))
		{
			HoldObjectiveElapsedSeconds = 0.0f;
			LastScenarioInteractionMessage = TEXT("Move to HOLD.");
			DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
			return false;
		}

		HoldObjectiveElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
		const float RemainingSeconds = FMath::Max(0.0f, DemoHoldRequiredSeconds - HoldObjectiveElapsedSeconds);
		LastScenarioInteractionMessage = FString::Printf(TEXT("Hold %.1fs."), RemainingSeconds);
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);

		if (HoldObjectiveElapsedSeconds >= DemoHoldRequiredSeconds)
		{
			HoldObjectiveElapsedSeconds = 0.0f;
			return TryCompleteActiveObjective(TEXT("Hold complete."));
		}

		return false;
	}

	if (Objective.ObjectiveId == TEXT("interact_console"))
	{
		LastScenarioInteractionMessage = TEXT("Press E to interact with CONSOLE.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		HoldObjectiveElapsedSeconds = 0.0f;
		return false;
	}

	HoldObjectiveElapsedSeconds = 0.0f;
	return false;
}

bool AMissionScenarioDemoPawn::InteractWithCurrentObjective()
{
	AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		LastScenarioInteractionMessage = TEXT("No mission scenario demo actor was found in the current world.");
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	UMissionScenarioInstance* ScenarioInstance = DemoActor->GetScenarioInstance();
	if (ScenarioInstance == nullptr || !ScenarioInstance->IsRunning())
	{
		LastScenarioInteractionMessage = TEXT("Mission is not active.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	FMissionScenarioObjectiveDefinition Objective;
	if (!ScenarioInstance->GetCurrentObjective(Objective))
	{
		LastScenarioInteractionMessage = TEXT("No active objective.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	if (Objective.ObjectiveId != TEXT("interact_console"))
	{
		LastScenarioInteractionMessage = TEXT("Follow the marker objective.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	if (!IsNearObjectiveMarker(Objective.ObjectiveId, ConsoleInteractionRadius))
	{
		LastScenarioInteractionMessage = TEXT("Press E to interact with CONSOLE.");
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("%s"), *LastScenarioInteractionMessage);
		return false;
	}

	return TryCompleteActiveObjective(TEXT("Console interaction complete."));
}

FString AMissionScenarioDemoPawn::GetLastScenarioInteractionMessage() const
{
	return LastScenarioInteractionMessage;
}

AMissionScenarioDemoActor* AMissionScenarioDemoPawn::FindDemoScenarioActor() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TArray<AActor*> DemoActors;
	UGameplayStatics::GetAllActorsOfClass(World, AMissionScenarioDemoActor::StaticClass(), DemoActors);
	for (AActor* Actor : DemoActors)
	{
		if (AMissionScenarioDemoActor* DemoActor = Cast<AMissionScenarioDemoActor>(Actor))
		{
			return DemoActor;
		}
	}

	return nullptr;
}

AActor* AMissionScenarioDemoPawn::FindObjectiveMarker(const FString& ObjectiveId) const
{
	const FName MarkerLabel = GetMarkerLabelForObjective(ObjectiveId);
	if (MarkerLabel.IsNone())
	{
		return nullptr;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr && Actor->GetActorLabel() == MarkerLabel.ToString())
		{
			return Actor;
		}
	}

	return nullptr;
}

bool AMissionScenarioDemoPawn::IsNearObjectiveMarker(const FString& ObjectiveId, const float AcceptanceRadius) const
{
	const AActor* Marker = FindObjectiveMarker(ObjectiveId);
	if (Marker == nullptr)
	{
		return false;
	}

	const float Distance2D = FVector::Dist2D(GetActorLocation(), Marker->GetActorLocation());
	return Distance2D <= AcceptanceRadius;
}

bool AMissionScenarioDemoPawn::TryCompleteActiveObjective(const FString& CompletionMessage)
{
	if (!AdvanceScenarioObjective())
	{
		return false;
	}

	LastScenarioInteractionMessage = CompletionMessage;
	if (AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor())
	{
		DemoActor->SetDemoStatusMessage(LastScenarioInteractionMessage);
	}
	UE_LOG(LogMissionScenarioRuntime, Log, TEXT("%s"), *LastScenarioInteractionMessage);
	return true;
}

void AMissionScenarioDemoPawn::HandleAdvanceScenarioObjectiveInput()
{
	InteractWithCurrentObjective();
}
