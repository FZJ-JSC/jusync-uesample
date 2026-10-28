#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "JusyncPlaybackHUDPlayerController.generated.h"

class AJUSYNCFileSpawnerActor;
class UJusyncPlaybackWidget;
class UJusyncPathlineWidget;
class UJusyncShortcutHintWidget;

UCLASS()
class AJusyncPlaybackHUDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AJusyncPlaybackHUDPlayerController();

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	TSubclassOf<UJusyncPlaybackWidget> PlaybackWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	TSubclassOf<UJusyncPathlineWidget> PathlineWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	TSubclassOf<UJusyncShortcutHintWidget> ShortcutHintClass;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	FKey ToggleKey = EKeys::T;

	UPROPERTY(EditDefaultsOnly, Category = "JUSYNC|Playback HUD")
	FKey PathlineToggleKey = EKeys::P;

	bool HandlePanelToggleKey(const FKey& Key, bool bFromPreInput = false);

	UFUNCTION(BlueprintCallable, Category = "JUSYNC|Playback HUD")
	void TogglePanel();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

private:
	void TogglePathlinePanel();
	void ApplyPanelInputMode();

	void HandlePreInputKey(const FKeyEvent& KeyEvent);
	bool IsGameViewportFocused() const;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPlaybackWidget> PlaybackWidget;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncPathlineWidget> PathlineWidget;

	UPROPERTY(Transient)
	TObjectPtr<UJusyncShortcutHintWidget> ShortcutHintWidget;

	AJUSYNCFileSpawnerActor* Spawner = nullptr;
	bool bPanelVisible = false;
	bool bPathlinePanelVisible = false;

	FDelegateHandle PreInputKeyHandle;
	FKey PreInputHandledKey;
	uint64 PreInputHandledFrame = MAX_uint64;
};
