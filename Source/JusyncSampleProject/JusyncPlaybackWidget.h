#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JusyncPlaybackWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UComboBoxString;
class UTextBlock;
class USlider;

/**
 * In-game transport for the sample cache on AJUSYNCFileSpawnerActor: scrub
 * bar, step buttons, LIVE resume and a timestep picker.
 *
 * Mirrors what the Details-panel scrubber does in the editor, but only with
 * runtime widgets so it also works in PIE and packaged builds. The layout is
 * built here in code; the maths and label formats live in
 * FJusyncPlaybackHUDModel so they stay testable.
 */
UCLASS()
class UJusyncPlaybackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The widget starts hidden; the player controller decides when to show it. */
	void BindSpawner(AJUSYNCFileSpawnerActor* InSpawner);

	/**
	 * Builds the widget tree and binds every handler, once. NativeOnInitialized
	 * calls it, but UUserWidget only fires that when a player context exists, so
	 * BindSpawner and the construction test call it directly instead.
	 */
	void EnsureLayoutBuilt();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Lets Tab/P toggle the panels even while focus sits inside one. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleSliderChanged(float Value);

	UFUNCTION()
	void HandleSliderCaptureBegin();

	UFUNCTION()
	void HandleSliderCaptureEnd();

	UFUNCTION()
	void HandlePrevClicked();

	UFUNCTION()
	void HandleNextClicked();

	UFUNCTION()
	void HandleLiveClicked();

	UFUNCTION()
	void HandlePlayPauseClicked();

	UFUNCTION()
	void HandlePathlineClicked();

	UFUNCTION()
	void HandleTimestepSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	void BuildLayout();
	UTextBlock* MakeLabel(const FName Name, const FText& Label);
	UButton* MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
		TObjectPtr<UTextBlock>* OutCaption = nullptr);

	/** Pull count / index / sender time out of the spawner and repaint. */
	void Refresh();

	/** Only rebuilds the dropdown when the cached timeline actually changed. */
	void SyncTimestepOptions(int32 SampleCount);

	static FString MakeLiveOptionLabel();

	UPROPERTY(Transient)
	TObjectPtr<AJUSYNCFileSpawnerActor> Spawner;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ScrubSlider;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> TimestepCombo;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrevButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NextButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LiveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayPauseButton;

	/** Caption of PlayPauseButton; the label flips with the transport state. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayPauseCaption;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PathlineButton;

	/** Caption of PathlineButton; the label reflects bEnablePathlines. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathlineCaption;

	/** Guards the programmatic SetValue / SetSelectedIndex calls. */
	bool bApplyingFromCode = false;
	bool bDraggingSlider = false;
	bool bLayoutBuilt = false;

	float RefreshAccumulator = 0.0f;
	FString LastTimelineSignature;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
