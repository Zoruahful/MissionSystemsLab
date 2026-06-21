#include "MissionScenarioDemoHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "MissionScenarioDemoActor.h"
#include "MissionScenarioInstance.h"
#include "MissionScenarioTypes.h"

namespace
{
FString RunStateToHudLabel(const EMissionScenarioRunState RunState)
{
	switch (RunState)
	{
	case EMissionScenarioRunState::Running:
		return TEXT("MISSION ACTIVE");
	case EMissionScenarioRunState::Completed:
		return TEXT("MISSION COMPLETE");
	case EMissionScenarioRunState::Failed:
		return TEXT("MISSION FAILED");
	case EMissionScenarioRunState::Ready:
		return TEXT("READY");
	default:
		return TEXT("STANDBY");
	}
}
}

void AMissionScenarioDemoHUD::DrawHUD()
{
	Super::DrawHUD();

	if (Canvas == nullptr || GEngine == nullptr)
	{
		return;
	}

	const AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		return;
	}

	const FMissionScenarioRuntimeSnapshot Snapshot = DemoActor->GetDemoSnapshot();
	const FString Header = RunStateToHudLabel(Snapshot.RunState);
	const FString Objective = BuildObjectiveLine();
	const FString Instruction = BuildInstructionLine();
	const FString Progress = FString::Printf(TEXT("Progress %d / %d"), Snapshot.CompletedObjectiveCount, Snapshot.TotalObjectiveCount);
	const FString ParseSummary = DemoActor->GetScenarioParseSummary();

	const FVector2D PanelPosition(32.0f, 32.0f);
	const FVector2D PanelSize(460.0f, 178.0f);
	const FLinearColor PanelColor(0.02f, 0.025f, 0.03f, 0.78f);
	const FLinearColor AccentColor(0.1f, 0.65f, 1.0f, 1.0f);
	const FLinearColor TextColor(0.96f, 0.98f, 1.0f, 1.0f);
	const FLinearColor MutedTextColor(0.72f, 0.78f, 0.84f, 1.0f);

	FCanvasTileItem PanelTile(PanelPosition, PanelSize, PanelColor);
	PanelTile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(PanelTile);

	FCanvasTileItem AccentTile(PanelPosition, FVector2D(6.0f, PanelSize.Y), AccentColor);
	AccentTile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(AccentTile);

	UFont* Font = GEngine->GetMediumFont();
	UFont* SmallFont = GEngine->GetSmallFont();

	FCanvasTextItem HeaderText(PanelPosition + FVector2D(22.0f, 14.0f), FText::FromString(Header), Font, AccentColor);
	HeaderText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(HeaderText);

	FCanvasTextItem ObjectiveText(PanelPosition + FVector2D(22.0f, 48.0f), FText::FromString(Objective), Font, TextColor);
	ObjectiveText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(ObjectiveText);

	FCanvasTextItem InstructionText(PanelPosition + FVector2D(22.0f, 86.0f), FText::FromString(Instruction), SmallFont, TextColor);
	InstructionText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(InstructionText);

	FCanvasTextItem ProgressText(PanelPosition + FVector2D(22.0f, 116.0f), FText::FromString(Progress), SmallFont, MutedTextColor);
	ProgressText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(ProgressText);

	FCanvasTextItem ParseText(PanelPosition + FVector2D(22.0f, 144.0f), FText::FromString(ParseSummary), SmallFont, MutedTextColor);
	ParseText.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(ParseText);
}

FString AMissionScenarioDemoHUD::BuildObjectiveLine() const
{
	const AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		return TEXT("Objective: Locate mission");
	}

	const UMissionScenarioInstance* ScenarioInstance = DemoActor->GetScenarioInstance();
	if (ScenarioInstance == nullptr)
	{
		return TEXT("Objective: Stand by");
	}

	FMissionScenarioObjectiveDefinition Objective;
	if (!ScenarioInstance->GetCurrentObjective(Objective))
	{
		return ScenarioInstance->IsScenarioComplete()
			? TEXT("Objective: All objectives complete")
			: TEXT("Objective: Stand by");
	}

	const FString ObjectiveName = Objective.DisplayName.IsEmpty() ? Objective.ObjectiveId : Objective.DisplayName;
	return FString::Printf(TEXT("Objective: %s"), *ObjectiveName);
}

FString AMissionScenarioDemoHUD::BuildInstructionLine() const
{
	const AMissionScenarioDemoActor* DemoActor = FindDemoScenarioActor();
	if (DemoActor == nullptr)
	{
		return TEXT("Find the mission area.");
	}

	const FString StatusMessage = DemoActor->GetDemoStatusMessage();
	if (!StatusMessage.IsEmpty())
	{
		return StatusMessage;
	}

	return TEXT("Follow the active marker.");
}

AMissionScenarioDemoActor* AMissionScenarioDemoHUD::FindDemoScenarioActor() const
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
