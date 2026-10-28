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
	 * The pre-input hook calls this first; the widgets and InputKey also call
	 * it as a fallback. A per-frame guard makes one physical press toggle
	 * exactly once even when all three paths see the same event.
	 */
	bool HandlePanelToggleKey(const FKey& Key, bool bFromPreInput = false);

	/** Show/hide the playback transport, the same action the Tab key runs. */
	UFUNCTION(BlueprintCallable, Category = "JUSYNC|Playback HUD")
	void TogglePanel();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void TogglePathlinePanel();
	void ApplyPanelInputMode();

	/** Runs before Slate focus navigation, so Tab/P still toggle the panels
	 *  when a HUD button or slider owns keyboard focus. */
	void HandlePreInputKey(const FKeyEvent& KeyEvent);
	bool IsGameViewportFocused() const;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPlaybackWidget> PlaybackWidget;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPathlineWidget> PathlineWidget;

	AJUSYNCFileSpawnerActor* Spawner = nullptr;
	bool bPanelVisible = false;
	bool bPathlinePanelVisible = false;

	FDelegateHandle PreInputKeyHandle;
	FKey PreInputHandledKey;
	uint64 PreInputHandledFrame = MAX_uint64;
};
