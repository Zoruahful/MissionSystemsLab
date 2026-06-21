#include "MissionScenarioDemoGameMode.h"

#include "MissionScenarioDemoHUD.h"
#include "MissionScenarioDemoPawn.h"

AMissionScenarioDemoGameMode::AMissionScenarioDemoGameMode()
{
	DefaultPawnClass = AMissionScenarioDemoPawn::StaticClass();
	HUDClass = AMissionScenarioDemoHUD::StaticClass();
}
