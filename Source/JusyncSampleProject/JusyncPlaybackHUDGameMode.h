#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JusyncPlaybackHUDGameMode.generated.h"

/**
 * Default game mode for the sample: installs the player controller that owns
 * the JUSYNC playback HUD. Wired through GlobalDefaultGameMode in
 * Config/DefaultEngine.ini so no map or Blueprint asset has to change.
 */
UCLASS()
class AJusyncPlaybackHUDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJusyncPlaybackHUDGameMode();
};
