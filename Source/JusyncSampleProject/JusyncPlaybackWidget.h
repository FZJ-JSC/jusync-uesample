#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JusyncHUDPanelReveal.h"
#include "JusyncPlaybackWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class USlider;

UCLASS()
class UJusyncPlaybackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindSpawner(AJUSYNCFileSpawnerActor* InSpawner);

	void EnsureLayoutBuilt();

	void Open();

	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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
	void HandleRateMinus();

	UFUNCTION()
	void HandleRatePlus();

	UFUNCTION()
	void HandleLoopClicked();

	void BuildLayout();
	void ApplyReveal();
	UTextBlock* MakeLabel(const FName Name, const FText& Label);
	UBorder* MakeRoundedBorder(const FName Name, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius, float OutlineWidth,
		const FMargin& InPadding = FMargin(0.0f));
	UButton* MakeSwitch(const FName Name, TObjectPtr<UBorder>& OutThumb);
	void SetSwitchState(UButton* Button, UBorder* Thumb, bool bOn);

	void Refresh();

	void TickCachedPlayback(float DeltaSeconds);

	void TickSliderVisual(float DeltaSeconds);

	void StartCachedPlaybackFrom(bool bFromLiveEdge);

	UPROPERTY(Transient)
	TObjectPtr<AJUSYNCFileSpawnerActor> Spawner;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HUDPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HUDDot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HUDStateText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TransportBar;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ScrubSlider;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ScrubVisual;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubTrack;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubFill;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubThumb;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubThumbRing;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ScrubBubble;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScrubBubbleText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubGlow;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrubRipple;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StepText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StepCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RateValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrevButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NextButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LiveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayPauseButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PlayPauseIcon;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RateMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RatePlusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoopSwitch;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LoopThumb;

	bool bApplyingFromCode = false;
	bool bDraggingSlider = false;
	bool bLayoutBuilt = false;

	float ScrubThumbScale = 1.0f;
	float ScrubBubbleAlpha = 0.0f;

	FJusyncPanelReveal Reveal;
	TArray<FJusyncGlassBlur> GlassBlurs;

	bool bCachedPlaybackRunning = false;
	float CachedPlaybackAccumulator = 0.0f;
	// Sample count when the replay started; a growing count means the sender is
	// still streaming, and replaying the buffer would just hide the live frames.
	int32 CachedPlaybackStartCount = 0;

	bool bAutoLoopPlaybackSuppressed = false;

	float RefreshAccumulator = 0.0f;

	double LastWidgetTickSeconds = 0.0;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
