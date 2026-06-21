#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MissionScenarioDemoHUD.generated.h"

UCLASS()
class MISSIONSCENARIORUNTIME_API AMissionScenarioDemoHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	FString BuildObjectiveLine() const;
	FString BuildInstructionLine() const;
	class AMissionScenarioDemoActor* FindDemoScenarioActor() const;
};
