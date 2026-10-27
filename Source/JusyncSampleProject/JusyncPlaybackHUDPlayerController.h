#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "JusyncPlaybackHUDPlayerController.generated.h"

class AJUSYNCFileSpawnerActor;
class UJusyncPlaybackWidget;
class UJusyncPathlineWidget;

/**
 * Shows the JUSYNC playback transport on demand.
 *
 * The panel is hidden at start; ToggleKey hands the mouse to the widget and
 * back again, so camera or other level input is only interrupted while the
 * transport is actually in use. The pathline controls live in their own
 * panel on its own key (PathlineToggleKey) so the transport stays a strip.
 */
UCLASS()
class AJusyncPlaybackHUDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AJusyncPlaybackHUDPlayerController();

	/** Defaults to the C++ widget; set to a Blueprint subclass to restyle. */
	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	TSubclassOf<UJusyncPlaybackWidget> PlaybackWidgetClass;

	/** Defaults to the C++ pathline panel; set to a Blueprint subclass to restyle. */
	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	TSubclassOf<UJusyncPathlineWidget> PathlineWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	FKey ToggleKey = EKeys::Tab;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	FKey PathlineToggleKey = EKeys::P;

	/**
	 * Toggles the panel whose key matches, returning whether Key was one.
	 * The widgets call this from NativeOnKeyDown because Slate routes keys
	 * along the focused widget's path: while the caret sits inside a panel,
	 * the controller's InputKey never runs and the toggles would seem dead.
	 */
	bool HandlePanelToggleKey(const FKey& Key);

protected:
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void TogglePanel();
	void TogglePathlinePanel();
	void ApplyPanelInputMode();

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPlaybackWidget> PlaybackWidget;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPathlineWidget> PathlineWidget;

	AJUSYNCFileSpawnerActor* Spawner = nullptr;
	bool bPanelVisible = false;
	bool bPathlinePanelVisible = false;
};
