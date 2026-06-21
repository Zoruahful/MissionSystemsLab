#include "MissionScenarioRuntimeLibrary.h"

#include "Dom/JsonObject.h"
#include "MissionScenarioInstance.h"
#include "MissionScenarioRuntime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

namespace
{
constexpr const TCHAR* ScenarioContractVersion = TEXT("msl.scenario.v1");

FString Trimmed(FString Value)
{
	Value.TrimStartAndEndInline();
	return Value;
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& Context,
	FString& OutValue,
	FString& OutError)
{
	if (!Object.IsValid() || !Object->TryGetStringField(FieldName, OutValue))
	{
		OutError = FString::Printf(TEXT("%s requires string field '%s'."), *Context, FieldName);
		return false;
	}

	OutValue = Trimmed(OutValue);
	if (OutValue.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s requires non-empty field '%s'."), *Context, FieldName);
		return false;
	}

	return true;
}

bool ReadOptionalNonNegativeNumber(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& Context,
	float& OutValue,
	FString& OutError)
{
	double NumberValue = static_cast<double>(OutValue);
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, NumberValue))
	{
		return true;
	}

	if (NumberValue < 0.0)
	{
		OutError = FString::Printf(TEXT("%s field '%s' must be zero or greater."), *Context, FieldName);
		return false;
	}

	OutValue = static_cast<float>(NumberValue);
	return true;
}

bool ReadOptionalPositiveInteger(
	const TSharedPtr<FJsonObject>& Object,
	const TCHAR* FieldName,
	const FString& Context,
	int32& OutValue,
	FString& OutError)
{
	double NumberValue = static_cast<double>(OutValue);
	if (!Object.IsValid() || !Object->TryGetNumberField(FieldName, NumberValue))
	{
		return true;
	}

	const int32 RoundedValue = FMath::RoundToInt(NumberValue);
	if (RoundedValue < 1 || !FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue)))
	{
		OutError = FString::Printf(TEXT("%s field '%s' must be a positive integer."), *Context, FieldName);
		return false;
	}

	OutValue = RoundedValue;
	return true;
}

bool TryParseObjectiveType(const FString& TypeText, EMissionScenarioObjectiveType& OutType)
{
	if (TypeText.Equals(TEXT("ReachLocation"), ESearchCase::IgnoreCase))
	{
		OutType = EMissionScenarioObjectiveType::ReachLocation;
		return true;
	}

	if (TypeText.Equals(TEXT("Interact"), ESearchCase::IgnoreCase))
	{
		OutType = EMissionScenarioObjectiveType::Interact;
		return true;
	}

	if (TypeText.Equals(TEXT("Wait"), ESearchCase::IgnoreCase))
	{
		OutType = EMissionScenarioObjectiveType::Wait;
		return true;
	}

	if (TypeText.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
	{
		OutType = EMissionScenarioObjectiveType::Custom;
		return true;
	}

	return false;
}

bool ParseObjective(
	const TSharedPtr<FJsonObject>& ObjectiveObject,
	const FString& Context,
	FMissionScenarioObjectiveDefinition& OutObjective,
	FString& OutError)
{
	if (!ReadRequiredString(ObjectiveObject, TEXT("objectiveId"), Context, OutObjective.ObjectiveId, OutError) ||
		!ReadRequiredString(ObjectiveObject, TEXT("displayName"), Context, OutObjective.DisplayName, OutError))
	{
		return false;
	}

	FString TypeText;
	if (!ReadRequiredString(ObjectiveObject, TEXT("type"), Context, TypeText, OutError))
	{
		return false;
	}

	if (!TryParseObjectiveType(TypeText, OutObjective.ObjectiveType))
	{
		OutError = FString::Printf(
			TEXT("%s has unsupported objective type '%s'. Expected ReachLocation, Interact, Wait, or Custom."),
			*Context,
			*TypeText);
		return false;
	}

	ObjectiveObject->TryGetStringField(TEXT("targetTag"), OutObjective.TargetTag);
	OutObjective.TargetTag = Trimmed(OutObjective.TargetTag);

	if (!ReadOptionalPositiveInteger(ObjectiveObject, TEXT("requiredCount"), Context, OutObjective.RequiredCount, OutError) ||
		!ReadOptionalNonNegativeNumber(ObjectiveObject, TEXT("timeLimitSeconds"), Context, OutObjective.TimeLimitSeconds, OutError))
	{
		return false;
	}

	const bool bRequiresTarget = OutObjective.ObjectiveType == EMissionScenarioObjectiveType::ReachLocation ||
		OutObjective.ObjectiveType == EMissionScenarioObjectiveType::Interact;
	if (bRequiresTarget && OutObjective.TargetTag.IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s objective type requires non-empty field 'targetTag'."), *Context);
		return false;
	}

	if (OutObjective.ObjectiveType == EMissionScenarioObjectiveType::Wait && OutObjective.TimeLimitSeconds <= 0.0f)
	{
		OutError = FString::Printf(TEXT("%s Wait objective requires 'timeLimitSeconds' greater than zero."), *Context);
		return false;
	}

	return true;
}

bool ParsePhase(
	const TSharedPtr<FJsonObject>& PhaseObject,
	const FString& Context,
	FMissionScenarioPhaseDefinition& OutPhase,
	FString& OutError)
{
	if (!ReadRequiredString(PhaseObject, TEXT("phaseId"), Context, OutPhase.PhaseId, OutError) ||
		!ReadRequiredString(PhaseObject, TEXT("displayName"), Context, OutPhase.DisplayName, OutError))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* ObjectiveValues = nullptr;
	if (!PhaseObject->TryGetArrayField(TEXT("objectives"), ObjectiveValues) || ObjectiveValues == nullptr || ObjectiveValues->IsEmpty())
	{
		OutError = FString::Printf(TEXT("%s requires a non-empty 'objectives' array."), *Context);
		return false;
	}

	OutPhase.Objectives.Reserve(ObjectiveValues->Num());
	for (int32 ObjectiveIndex = 0; ObjectiveIndex < ObjectiveValues->Num(); ++ObjectiveIndex)
	{
		const TSharedPtr<FJsonObject> ObjectiveObject = (*ObjectiveValues)[ObjectiveIndex]->AsObject();
		const FString ObjectiveContext = FString::Printf(TEXT("%s.objectives[%d]"), *Context, ObjectiveIndex);
		if (!ObjectiveObject.IsValid())
		{
			OutError = FString::Printf(TEXT("%s must be an object."), *ObjectiveContext);
			return false;
		}

		FMissionScenarioObjectiveDefinition Objective;
		if (!ParseObjective(ObjectiveObject, ObjectiveContext, Objective, OutError))
		{
			return false;
		}

		OutPhase.Objectives.Add(MoveTemp(Objective));
	}

	return true;
}
}

FString UMissionScenarioRuntimeLibrary::GetMissionScenarioRuntimeVersion()
{
	return TEXT("MissionScenarioRuntime 0.1.0");
}

bool UMissionScenarioRuntimeLibrary::IsMissionScenarioRuntimeAvailable()
{
	return true;
}

FString UMissionScenarioRuntimeLibrary::GetScenarioContractVersion()
{
	return ScenarioContractVersion;
}

bool UMissionScenarioRuntimeLibrary::ParseScenarioJson(
	const FString& ScenarioJson,
	FMissionScenarioDefinition& OutScenario,
	FString& OutError)
{
	OutScenario = FMissionScenarioDefinition();
	OutError.Reset();

	if (Trimmed(ScenarioJson).IsEmpty())
	{
		OutError = TEXT("Scenario JSON cannot be empty.");
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ScenarioJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("Scenario JSON must parse as an object.");
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
		return false;
	}

	const FString RootContext = TEXT("scenario");
	if (!ReadRequiredString(RootObject, TEXT("schemaVersion"), RootContext, OutScenario.SchemaVersion, OutError) ||
		!ReadRequiredString(RootObject, TEXT("scenarioId"), RootContext, OutScenario.ScenarioId, OutError) ||
		!ReadRequiredString(RootObject, TEXT("displayName"), RootContext, OutScenario.DisplayName, OutError))
	{
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
		return false;
	}

	if (!OutScenario.SchemaVersion.Equals(ScenarioContractVersion, ESearchCase::CaseSensitive))
	{
		OutError = FString::Printf(
			TEXT("scenario schemaVersion '%s' is not supported. Expected '%s'."),
			*OutScenario.SchemaVersion,
			ScenarioContractVersion);
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
		return false;
	}

	RootObject->TryGetStringField(TEXT("briefing"), OutScenario.Briefing);
	OutScenario.Briefing = Trimmed(OutScenario.Briefing);

	const TArray<TSharedPtr<FJsonValue>>* PhaseValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("phases"), PhaseValues) || PhaseValues == nullptr || PhaseValues->IsEmpty())
	{
		OutError = TEXT("scenario requires a non-empty 'phases' array.");
		UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
		return false;
	}

	OutScenario.Phases.Reserve(PhaseValues->Num());
	for (int32 PhaseIndex = 0; PhaseIndex < PhaseValues->Num(); ++PhaseIndex)
	{
		const TSharedPtr<FJsonObject> PhaseObject = (*PhaseValues)[PhaseIndex]->AsObject();
		const FString PhaseContext = FString::Printf(TEXT("scenario.phases[%d]"), PhaseIndex);
		if (!PhaseObject.IsValid())
		{
			OutError = FString::Printf(TEXT("%s must be an object."), *PhaseContext);
			UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
			return false;
		}

		FMissionScenarioPhaseDefinition Phase;
		if (!ParsePhase(PhaseObject, PhaseContext, Phase, OutError))
		{
			UE_LOG(LogMissionScenarioRuntime, Warning, TEXT("Scenario parse failed: %s"), *OutError);
			return false;
		}

		OutScenario.Phases.Add(MoveTemp(Phase));
	}

	UE_LOG(
		LogMissionScenarioRuntime,
		Log,
		TEXT("Parsed scenario '%s' with %d phase(s)."),
		*OutScenario.ScenarioId,
		OutScenario.Phases.Num());
	return true;
}

FString UMissionScenarioRuntimeLibrary::GetScenarioJsonValidationError(const FString& ScenarioJson)
{
	FMissionScenarioDefinition IgnoredScenario;
	FString Error;
	ParseScenarioJson(ScenarioJson, IgnoredScenario, Error);
	return Error;
}

UMissionScenarioInstance* UMissionScenarioRuntimeLibrary::CreateMissionScenarioInstance(UObject* WorldContextObject)
{
	UObject* Outer = WorldContextObject != nullptr ? WorldContextObject : GetTransientPackage();
	return NewObject<UMissionScenarioInstance>(Outer);
}
