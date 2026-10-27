#include "JusyncPlaybackHUDGameMode.h"

#include "JusyncPlaybackHUDPlayerController.h"

AJusyncPlaybackHUDGameMode::AJusyncPlaybackHUDGameMode()
{
	PlayerControllerClass = AJusyncPlaybackHUDPlayerController::StaticClass();
}
