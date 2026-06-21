#include "MissionScenarioInstance.h"

#include "Dom/JsonObject.h"
#include "Misc/DateTime.h"
#include "MissionScenarioRuntime.h"
#include "MissionScenarioRuntimeLibrary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr const TCHAR* EventLogContractVersion = TEXT("msl.eventlog.v1");

FString RunStateToString(const EMissionScenarioRunState RunState)
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

FString EventTypeToString(const EMissionScenarioEventType EventType)
{
	switch (EventType)
	{
	case EMissionScenarioEventType::Initialized:
		return TEXT("Initialized");
	case EMissionScenarioEventType::Started:
		return TEXT("Started");
	case EMissionScenarioEventType::ObjectiveCompleted:
		return TEXT("ObjectiveCompleted");
	case EMissionScenarioEventType::ObjectiveAdvanced:
		return TEXT("ObjectiveAdvanced");
	case EMissionScenarioEventType::Completed:
		return TEXT("Completed");
	case EMissionScenarioEventType::Failed:
		return TEXT("Failed");
	case EMissionScenarioEventType::Reset:
		return TEXT("Reset");
	case EMissionScenarioEventType::InvalidAction:
		return TEXT("InvalidAction");
	default:
		return TEXT("Unknown");
	}
}

bool ValidateScenarioDefinition(const FMissionScenarioDefinition& ScenarioDefinition, FString& OutError)
{
	if (ScenarioDefinition.ScenarioId.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Scenario definition requires a non-empty ScenarioId.");
		return false;
	}

	if (ScenarioDefinition.DisplayName.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Scenario definition requires a non-empty DisplayName.");
		return false;
	}

	if (ScenarioDefinition.Phases.IsEmpty())
	{
		OutError = TEXT("Scenario definition requires at least one phase.");
		return false;
	}

	for (int32 PhaseIndex = 0; PhaseIndex < ScenarioDefinition.Phases.Num(); ++PhaseIndex)
	{
		const FMissionScenarioPhaseDefinition& Phase = ScenarioDefinition.Phases[PhaseIndex];
		const FString PhaseContext = FString::Printf(TEXT("Scenario phase %d"), PhaseIndex);

		if (Phase.PhaseId.TrimStartAndEnd().IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s requires a non-empty PhaseId."), *PhaseContext);
			return false;
		}

		if (Phase.Objectives.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s requires at least one objective."), *PhaseContext);
			return false;
		}

		for (int32 ObjectiveIndex = 0; ObjectiveIndex < Phase.Objectives.Num(); ++ObjectiveIndex)
		{
			const FMissionScenarioObjectiveDefinition& Objective = Phase.Objectives[ObjectiveIndex];
			const FString ObjectiveContext = FString::Printf(TEXT("%s objective %d"), *PhaseContext, ObjectiveIndex);

			if (Objective.ObjectiveId.TrimStartAndEnd().IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s requires a non-empty ObjectiveId."), *ObjectiveContext);
				return false;
			}

			if (Objective.RequiredCount < 1)
			{
				OutError = FString::Printf(TEXT("%s requires RequiredCount greater than zero."), *ObjectiveContext);
				return false;
			}

			if (Objective.TimeLimitSeconds < 0.0f)
			{
				OutError = FString::Printf(TEXT("%s requires TimeLimitSeconds zero or greater."), *ObjectiveContext);
				return false;
			}
		}
	}

	OutError.Reset();
	return true;
}
}

bool UMissionScenarioInstance::InitializeFromDefinition(
	const FMissionScenarioDefinition& InScenarioDefinition,
	FString& OutError)
{
	if (!ValidateScenarioDefinition(InScenarioDefinition, OutError))
	{
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario initialization failed: %s"), *OutError);
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	EventLog.Reset();
	ScenarioDefinition = InScenarioDefinition;
	RunState = EMissionScenarioRunState::Ready;
	CurrentPhaseIndex = INDEX_NONE;
	CurrentObjectiveIndex = INDEX_NONE;
	CompletedObjectiveCount = 0;
	FailureReason.Reset();
	OutError.Reset();
	AppendEvent(EMissionScenarioEventType::Initialized, TEXT("Initialized scenario definition."));

	UE_LOG(
		LogMissionScenarioRuntime,
		Log,
		TEXT("Initialized scenario instance '%s' with %d objective(s)."),
		*ScenarioDefinition.ScenarioId,
		GetTotalObjectiveCount());
	return true;
}

bool UMissionScenarioInstance::InitializeFromJson(const FString& ScenarioJson, FString& OutError)
{
	FMissionScenarioDefinition ParsedScenario;
	if (!UMissionScenarioRuntimeLibrary::ParseScenarioJson(ScenarioJson, ParsedScenario, OutError))
	{
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	return InitializeFromDefinition(ParsedScenario, OutError);
}

bool UMissionScenarioInstance::StartScenario(FString& OutError)
{
	if (RunState == EMissionScenarioRunState::Uninitialized)
	{
		OutError = TEXT("Cannot start scenario before initialization.");
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	if (RunState == EMissionScenarioRunState::Running)
	{
		OutError = TEXT("Scenario is already running.");
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	if (RunState == EMissionScenarioRunState::Completed || RunState == EMissionScenarioRunState::Failed)
	{
		OutError = TEXT("Reset scenario before starting it again.");
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	if (ScenarioDefinition.Phases.IsEmpty() || ScenarioDefinition.Phases[0].Objectives.IsEmpty())
	{
		OutError = TEXT("Scenario has no first objective to start.");
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	RunState = EMissionScenarioRunState::Running;
	CurrentPhaseIndex = 0;
	CurrentObjectiveIndex = 0;
	CompletedObjectiveCount = 0;
	FailureReason.Reset();
	OutError.Reset();

	const FMissionScenarioObjectiveDefinition& Objective = ScenarioDefinition.Phases[0].Objectives[0];
	AppendEvent(
		EMissionScenarioEventType::Started,
		TEXT("Started scenario."),
		ScenarioDefinition.Phases[0].PhaseId,
		Objective.ObjectiveId);
	UE_LOG(
		LogMissionScenarioRuntime,
		Log,
		TEXT("Started scenario '%s' at objective '%s'."),
		*ScenarioDefinition.ScenarioId,
		*Objective.ObjectiveId);
	return true;
}

bool UMissionScenarioInstance::CompleteCurrentObjective(FString& OutError)
{
	if (RunState != EMissionScenarioRunState::Running)
	{
		OutError = TEXT("Cannot complete an objective unless the scenario is running.");
		AppendEvent(EMissionScenarioEventType::InvalidAction, OutError);
		return false;
	}

	if (!HasCurrentObjective())
	{
		OutError = TEXT("Scenario has no active objective.");
		RunState = EMissionScenarioRunState::Failed;
		FailureReason = OutError;
		AppendEvent(EMissionScenarioEventType::Failed, OutError);
		return false;
	}

	const FMissionScenarioPhaseDefinition Phase = ScenarioDefinition.Phases[CurrentPhaseIndex];
	const FMissionScenarioObjectiveDefinition Objective = ScenarioDefinition.Phases[CurrentPhaseIndex].Objectives[CurrentObjectiveIndex];
	++CompletedObjectiveCount;
	AppendEvent(
		EMissionScenarioEventType::ObjectiveCompleted,
		FString::Printf(TEXT("Completed objective '%s'."), *Objective.ObjectiveId),
		Phase.PhaseId,
		Objective.ObjectiveId);
	AdvanceToNextObjectiveOrComplete();
	if (RunState == EMissionScenarioRunState::Completed)
	{
		AppendEvent(EMissionScenarioEventType::Completed, TEXT("Completed scenario."));
	}
	else if (HasCurrentObjective())
	{
		const FMissionScenarioPhaseDefinition& NextPhase = ScenarioDefinition.Phases[CurrentPhaseIndex];
		const FMissionScenarioObjectiveDefinition& NextObjective = NextPhase.Objectives[CurrentObjectiveIndex];
		AppendEvent(
			EMissionScenarioEventType::ObjectiveAdvanced,
			FString::Printf(TEXT("Advanced to objective '%s'."), *NextObjective.ObjectiveId),
			NextPhase.PhaseId,
			NextObjective.ObjectiveId);
	}
	OutError.Reset();

	UE_LOG(
		LogMissionScenarioRuntime,
		Log,
		TEXT("Completed objective '%s' in scenario '%s'. State is now %d."),
		*Objective.ObjectiveId,
		*ScenarioDefinition.ScenarioId,
		static_cast<int32>(RunState));
	return true;
}

void UMissionScenarioInstance::FailScenario(const FString& Reason)
{
	if (RunState == EMissionScenarioRunState::Uninitialized)
	{
		FailureReason = Reason.IsEmpty() ? TEXT("Scenario failed before initialization.") : Reason;
		RunState = EMissionScenarioRunState::Failed;
		AppendEvent(EMissionScenarioEventType::Failed, FailureReason);
		return;
	}

	if (RunState == EMissionScenarioRunState::Completed)
	{
		AppendEvent(EMissionScenarioEventType::InvalidAction, TEXT("Cannot fail a completed scenario."));
		return;
	}

	FailureReason = Reason.IsEmpty() ? TEXT("Scenario failed.") : Reason;
	RunState = EMissionScenarioRunState::Failed;
	CurrentPhaseIndex = INDEX_NONE;
	CurrentObjectiveIndex = INDEX_NONE;
	AppendEvent(EMissionScenarioEventType::Failed, FailureReason);

	UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario '%s' failed: %s"), *ScenarioDefinition.ScenarioId, *FailureReason);
}

void UMissionScenarioInstance::ResetScenario()
{
	if (RunState == EMissionScenarioRunState::Uninitialized)
	{
		return;
	}

	RunState = EMissionScenarioRunState::Ready;
	CurrentPhaseIndex = INDEX_NONE;
	CurrentObjectiveIndex = INDEX_NONE;
	CompletedObjectiveCount = 0;
	FailureReason.Reset();
	AppendEvent(EMissionScenarioEventType::Reset, TEXT("Reset scenario to ready."));
}

EMissionScenarioRunState UMissionScenarioInstance::GetRunState() const
{
	return RunState;
}

bool UMissionScenarioInstance::IsInitialized() const
{
	return RunState != EMissionScenarioRunState::Uninitialized;
}

bool UMissionScenarioInstance::IsRunning() const
{
	return RunState == EMissionScenarioRunState::Running;
}

bool UMissionScenarioInstance::IsScenarioComplete() const
{
	return RunState == EMissionScenarioRunState::Completed;
}

bool UMissionScenarioInstance::IsScenarioFailed() const
{
	return RunState == EMissionScenarioRunState::Failed;
}

FMissionScenarioDefinition UMissionScenarioInstance::GetScenarioDefinition() const
{
	return ScenarioDefinition;
}

bool UMissionScenarioInstance::GetCurrentPhase(FMissionScenarioPhaseDefinition& OutPhase) const
{
	if (!ScenarioDefinition.Phases.IsValidIndex(CurrentPhaseIndex))
	{
		OutPhase = FMissionScenarioPhaseDefinition();
		return false;
	}

	OutPhase = ScenarioDefinition.Phases[CurrentPhaseIndex];
	return true;
}

bool UMissionScenarioInstance::GetCurrentObjective(FMissionScenarioObjectiveDefinition& OutObjective) const
{
	if (!HasCurrentObjective())
	{
		OutObjective = FMissionScenarioObjectiveDefinition();
		return false;
	}

	OutObjective = ScenarioDefinition.Phases[CurrentPhaseIndex].Objectives[CurrentObjectiveIndex];
	return true;
}

FMissionScenarioRuntimeSnapshot UMissionScenarioInstance::GetSnapshot() const
{
	FMissionScenarioRuntimeSnapshot Snapshot;
	Snapshot.ScenarioId = ScenarioDefinition.ScenarioId;
	Snapshot.RunState = RunState;
	Snapshot.CurrentPhaseIndex = CurrentPhaseIndex;
	Snapshot.CurrentObjectiveIndex = CurrentObjectiveIndex;
	Snapshot.CompletedObjectiveCount = CompletedObjectiveCount;
	Snapshot.TotalObjectiveCount = GetTotalObjectiveCount();
	Snapshot.FailureReason = FailureReason;

	if (ScenarioDefinition.Phases.IsValidIndex(CurrentPhaseIndex))
	{
		const FMissionScenarioPhaseDefinition& Phase = ScenarioDefinition.Phases[CurrentPhaseIndex];
		Snapshot.CurrentPhaseId = Phase.PhaseId;

		if (Phase.Objectives.IsValidIndex(CurrentObjectiveIndex))
		{
			Snapshot.CurrentObjectiveId = Phase.Objectives[CurrentObjectiveIndex].ObjectiveId;
		}
	}

	return Snapshot;
}

TArray<FMissionScenarioEventRecord> UMissionScenarioInstance::GetEventLog() const
{
	return EventLog;
}

FString UMissionScenarioInstance::ExportEventLogToJson() const
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("schemaVersion"), EventLogContractVersion);
	RootObject->SetStringField(TEXT("sourceScenarioContractVersion"), UMissionScenarioRuntimeLibrary::GetScenarioContractVersion());
	RootObject->SetStringField(TEXT("scenarioId"), ScenarioDefinition.ScenarioId);
	RootObject->SetStringField(TEXT("runState"), RunStateToString(RunState));
	RootObject->SetNumberField(TEXT("eventCount"), EventLog.Num());

	TArray<TSharedPtr<FJsonValue>> EventValues;
	EventValues.Reserve(EventLog.Num());
	for (const FMissionScenarioEventRecord& EventRecord : EventLog)
	{
		const TSharedRef<FJsonObject> EventObject = MakeShared<FJsonObject>();
		EventObject->SetNumberField(TEXT("eventIndex"), EventRecord.EventIndex);
		EventObject->SetStringField(TEXT("timestampUtc"), EventRecord.TimestampUtc);
		EventObject->SetStringField(TEXT("scenarioId"), EventRecord.ScenarioId);
		EventObject->SetStringField(TEXT("eventType"), EventTypeToString(EventRecord.EventType));
		EventObject->SetStringField(TEXT("runState"), RunStateToString(EventRecord.RunState));
		EventObject->SetStringField(TEXT("phaseId"), EventRecord.PhaseId);
		EventObject->SetStringField(TEXT("objectiveId"), EventRecord.ObjectiveId);
		EventObject->SetNumberField(TEXT("completedObjectiveCount"), EventRecord.CompletedObjectiveCount);
		EventObject->SetNumberField(TEXT("totalObjectiveCount"), EventRecord.TotalObjectiveCount);
		EventObject->SetStringField(TEXT("message"), EventRecord.Message);
		EventValues.Add(MakeShared<FJsonValueObject>(EventObject));
	}
	RootObject->SetArrayField(TEXT("events"), EventValues);

	FString JsonOutput;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonOutput);
	FJsonSerializer::Serialize(RootObject, Writer);
	return JsonOutput;
}

bool UMissionScenarioInstance::HasCurrentObjective() const
{
	return ScenarioDefinition.Phases.IsValidIndex(CurrentPhaseIndex) &&
		ScenarioDefinition.Phases[CurrentPhaseIndex].Objectives.IsValidIndex(CurrentObjectiveIndex);
}

int32 UMissionScenarioInstance::GetTotalObjectiveCount() const
{
	int32 Total = 0;
	for (const FMissionScenarioPhaseDefinition& Phase : ScenarioDefinition.Phases)
	{
		Total += Phase.Objectives.Num();
	}
	return Total;
}

void UMissionScenarioInstance::AdvanceToNextObjectiveOrComplete()
{
	if (!ScenarioDefinition.Phases.IsValidIndex(CurrentPhaseIndex))
	{
		RunState = EMissionScenarioRunState::Completed;
		CurrentPhaseIndex = INDEX_NONE;
		CurrentObjectiveIndex = INDEX_NONE;
		return;
	}

	const FMissionScenarioPhaseDefinition& CurrentPhase = ScenarioDefinition.Phases[CurrentPhaseIndex];
	if (CurrentObjectiveIndex + 1 < CurrentPhase.Objectives.Num())
	{
		++CurrentObjectiveIndex;
		return;
	}

	if (CurrentPhaseIndex + 1 < ScenarioDefinition.Phases.Num())
	{
		++CurrentPhaseIndex;
		CurrentObjectiveIndex = 0;
		return;
	}

	RunState = EMissionScenarioRunState::Completed;
	CurrentPhaseIndex = INDEX_NONE;
	CurrentObjectiveIndex = INDEX_NONE;
}

void UMissionScenarioInstance::AppendEvent(
	const EMissionScenarioEventType EventType,
	const FString& Message,
	const FString& PhaseIdOverride,
	const FString& ObjectiveIdOverride)
{
	FMissionScenarioEventRecord EventRecord;
	EventRecord.EventIndex = EventLog.Num();
	EventRecord.TimestampUtc = FDateTime::UtcNow().ToIso8601();
	EventRecord.ScenarioId = ScenarioDefinition.ScenarioId;
	EventRecord.EventType = EventType;
	EventRecord.RunState = RunState;
	EventRecord.CompletedObjectiveCount = CompletedObjectiveCount;
	EventRecord.TotalObjectiveCount = GetTotalObjectiveCount();
	EventRecord.Message = Message;

	if (!PhaseIdOverride.IsEmpty())
	{
		EventRecord.PhaseId = PhaseIdOverride;
	}
	else if (ScenarioDefinition.Phases.IsValidIndex(CurrentPhaseIndex))
	{
		EventRecord.PhaseId = ScenarioDefinition.Phases[CurrentPhaseIndex].PhaseId;
	}

	if (!ObjectiveIdOverride.IsEmpty())
	{
		EventRecord.ObjectiveId = ObjectiveIdOverride;
	}
	else if (HasCurrentObjective())
	{
		EventRecord.ObjectiveId = ScenarioDefinition.Phases[CurrentPhaseIndex].Objectives[CurrentObjectiveIndex].ObjectiveId;
	}

	EventLog.Add(MoveTemp(EventRecord));
}
