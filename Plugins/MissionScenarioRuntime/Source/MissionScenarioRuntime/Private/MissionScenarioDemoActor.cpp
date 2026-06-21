#include "MissionScenarioDemoActor.h"

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "MissionScenarioRuntime.h"

namespace
{
constexpr int32 DemoCompletionGuardLimit = 100;

FString DemoRunStateToString(const EMissionScenarioRunState RunState)
{
	switch (RunState)
	{
	case EMissionScenarioRunState::Uninitialized:
		return TEXT("Uninitialized");
	case EMissionScenarioRunState::Ready:
		return TEXT("Ready");
	case EMissionScenarioRunState::Running:
		return TEXT("Running");
	case EMissionScenarioRunState::Completed:
		return TEXT("Completed");
	case EMissionScenarioRunState::Failed:
		return TEXT("Failed");
	default:
		return TEXT("Unknown");
	}
}

FString DemoRunStateToReviewLabel(const EMissionScenarioRunState RunState)
{
	switch (RunState)
	{
	case EMissionScenarioRunState::Uninitialized:
		return TEXT("STANDBY");
	case EMissionScenarioRunState::Ready:
		return TEXT("READY");
	case EMissionScenarioRunState::Running:
		return TEXT("MISSION ACTIVE");
	case EMissionScenarioRunState::Completed:
		return TEXT("MISSION COMPLETE");
	case EMissionScenarioRunState::Failed:
		return TEXT("MISSION FAILED");
	default:
		return TEXT("UNKNOWN");
	}
}
}

AMissionScenarioDemoActor::AMissionScenarioDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(RootComponent);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(12.0f);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	StatusText->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	StatusText->SetText(FText::FromString(TEXT("MISSION SCENARIO DEMO\nSTANDBY\nObjective: None\nProgress: 0/0")));
	StatusText->SetHiddenInGame(true);
	StatusText->SetVisibility(false);
	LastStatusMessage = TEXT("Press Play.");

	ScenarioJson = TEXT(R"json({
  "schemaVersion": "msl.scenario.v1",
  "scenarioId": "p1_demo_actor_flow",
  "displayName": "P1 Demo Actor Flow",
  "briefing": "Placeable runtime demo for scenario initialization, objective advancement, and event log export.",
  "phases": [
    {
      "phaseId": "phase_insert",
      "displayName": "Insertion",
      "objectives": [
        {
          "objectiveId": "reach_alpha",
          "displayName": "Reach Alpha",
          "type": "ReachLocation",
          "targetTag": "nav.alpha",
          "requiredCount": 1
        },
        {
          "objectiveId": "hold_position",
          "displayName": "Hold Position",
          "type": "Wait",
          "timeLimitSeconds": 5
        }
      ]
    },
    {
      "phaseId": "phase_extract",
      "displayName": "Extraction",
      "objectives": [
        {
          "objectiveId": "interact_console",
          "displayName": "Interact Console",
          "type": "Interact",
          "targetTag": "console.extract"
        }
      ]
    }
  ]
})json");
}

void AMissionScenarioDemoActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyStatusTextPresentation();

	FString Error;
	if (bInitializeOnBeginPlay && !InitializeDemoScenario(Error))
	{
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Demo actor initialization failed: %s"), *Error);
		return;
	}

	if (bStartOnBeginPlay && !StartDemoScenario(Error))
	{
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Demo actor start failed: %s"), *Error);
	}

	UpdateStatusText();
}

bool AMissionScenarioDemoActor::InitializeDemoScenario(FString& OutError)
{
	UMissionScenarioInstance* Instance = EnsureScenarioInstance();
	if (Instance == nullptr)
	{
		OutError = TEXT("Could not create mission scenario instance.");
		return false;
	}

	const bool bResult = Instance->InitializeFromJson(ScenarioJson, OutError);
	LastStatusMessage = bResult ? TEXT("Scenario initialized.") : OutError;
	UpdateStatusText();
	return bResult;
}

bool AMissionScenarioDemoActor::StartDemoScenario(FString& OutError)
{
	UMissionScenarioInstance* Instance = EnsureScenarioInstance();
	if (Instance == nullptr)
	{
		OutError = TEXT("Could not create mission scenario instance.");
		return false;
	}

	if (!Instance->IsInitialized())
	{
		if (!InitializeDemoScenario(OutError))
		{
			return false;
		}
	}

	const bool bResult = Instance->StartScenario(OutError);
	LastStatusMessage = bResult ? TEXT("Press E.") : OutError;
	UpdateStatusText();
	return bResult;
}

bool AMissionScenarioDemoActor::AdvanceDemoObjective(FString& OutError)
{
	if (ScenarioInstance == nullptr)
	{
		OutError = TEXT("Demo scenario has not been initialized.");
		return false;
	}

	const bool bResult = ScenarioInstance->CompleteCurrentObjective(OutError);
	if (bResult)
	{
		const FMissionScenarioRuntimeSnapshot Snapshot = ScenarioInstance->GetSnapshot();
		LastStatusMessage = ScenarioInstance->IsScenarioComplete()
			? TEXT("All objectives complete.")
			: TEXT("Objective advanced.");
	}
	else
	{
		LastStatusMessage = ScenarioInstance->IsScenarioComplete() ? TEXT("Scenario complete.") : OutError;
	}
	UpdateStatusText();
	return bResult;
}

bool AMissionScenarioDemoActor::RunDemoScenarioToCompletion(FString& OutError)
{
	if (!InitializeDemoScenario(OutError))
	{
		return false;
	}

	if (!StartDemoScenario(OutError))
	{
		return false;
	}

	int32 GuardCount = 0;
	while (ScenarioInstance != nullptr && ScenarioInstance->IsRunning())
	{
		if (++GuardCount > DemoCompletionGuardLimit)
		{
			OutError = TEXT("Demo scenario exceeded completion guard limit.");
			ScenarioInstance->FailScenario(OutError);
			return false;
		}

		if (!AdvanceDemoObjective(OutError))
		{
			return false;
		}
	}

	if (ScenarioInstance == nullptr || !ScenarioInstance->IsScenarioComplete())
	{
		OutError = TEXT("Demo scenario did not finish in the Completed state.");
		return false;
	}

	OutError.Reset();
	UpdateStatusText();
	return true;
}

void AMissionScenarioDemoActor::FailDemoScenario(const FString& Reason)
{
	UMissionScenarioInstance* Instance = EnsureScenarioInstance();
	if (Instance != nullptr)
	{
		Instance->FailScenario(Reason);
		LastStatusMessage = Reason.IsEmpty() ? TEXT("Scenario failed.") : Reason;
		UpdateStatusText();
	}
}

void AMissionScenarioDemoActor::ResetDemoScenario()
{
	if (ScenarioInstance != nullptr)
	{
		ScenarioInstance->ResetScenario();
		LastStatusMessage = TEXT("Scenario reset. Start it again to continue.");
		UpdateStatusText();
	}
}

void AMissionScenarioDemoActor::SetDemoStatusMessage(const FString& Message)
{
	LastStatusMessage = Message;
	UpdateStatusText();
}

FString AMissionScenarioDemoActor::GetDemoStatusMessage() const
{
	return LastStatusMessage;
}

FMissionScenarioRuntimeSnapshot AMissionScenarioDemoActor::GetDemoSnapshot() const
{
	if (ScenarioInstance == nullptr)
	{
		return FMissionScenarioRuntimeSnapshot();
	}

	return ScenarioInstance->GetSnapshot();
}

TArray<FMissionScenarioEventRecord> AMissionScenarioDemoActor::GetDemoEventLog() const
{
	if (ScenarioInstance == nullptr)
	{
		return TArray<FMissionScenarioEventRecord>();
	}

	return ScenarioInstance->GetEventLog();
}

FString AMissionScenarioDemoActor::ExportDemoEventLogToJson() const
{
	if (ScenarioInstance == nullptr)
	{
		return TEXT("{}");
	}

	return ScenarioInstance->ExportEventLogToJson();
}

UMissionScenarioInstance* AMissionScenarioDemoActor::GetScenarioInstance() const
{
	return ScenarioInstance;
}

UMissionScenarioInstance* AMissionScenarioDemoActor::EnsureScenarioInstance()
{
	if (ScenarioInstance == nullptr)
	{
		ScenarioInstance = NewObject<UMissionScenarioInstance>(this);
	}

	return ScenarioInstance;
}

FString AMissionScenarioDemoActor::BuildObjectiveDisplayText() const
{
	if (ScenarioInstance == nullptr)
	{
		return TEXT("None");
	}

	FMissionScenarioObjectiveDefinition Objective;
	if (!ScenarioInstance->GetCurrentObjective(Objective))
	{
		return ScenarioInstance->IsScenarioComplete() ? TEXT("All objectives complete") : TEXT("None");
	}

	if (!Objective.DisplayName.TrimStartAndEnd().IsEmpty())
	{
		return Objective.DisplayName;
	}

	return Objective.ObjectiveId.IsEmpty() ? TEXT("Unnamed Objective") : Objective.ObjectiveId;
}

void AMissionScenarioDemoActor::ApplyStatusTextPresentation() const
{
	if (StatusText == nullptr)
	{
		return;
	}

	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(12.0f);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 115.0f));
	StatusText->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	StatusText->SetHiddenInGame(true);
	StatusText->SetVisibility(false);
}

void AMissionScenarioDemoActor::UpdateStatusText()
{
	if (StatusText == nullptr)
	{
		return;
	}

	ApplyStatusTextPresentation();

	const FMissionScenarioRuntimeSnapshot Snapshot = GetDemoSnapshot();
	const FString ObjectiveText = BuildObjectiveDisplayText();
	const FString Message = LastStatusMessage.IsEmpty() ? DemoRunStateToString(Snapshot.RunState) : LastStatusMessage;
	const FString Text = FString::Printf(
		TEXT("%s\n%s  %d/%d\n%s"),
		*DemoRunStateToReviewLabel(Snapshot.RunState),
		*ObjectiveText,
		Snapshot.CompletedObjectiveCount,
		Snapshot.TotalObjectiveCount,
		*Message);

	StatusText->SetText(FText::FromString(Text));
}
