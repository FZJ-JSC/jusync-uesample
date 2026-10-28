#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JusyncHUDPanelReveal.h"
#include "JusyncPlaybackWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class USlider;

/**
 * In-game stream deck: the centred bottom transport, the top-left status
 * HUD, and the timestep readout. Geometry follows the dimensional drawing at
 * the 1920x1080 reference (W7 = 960, V1 = 24, H7 = 72, H8 = 44).
 *
 * The pathline / point-cloud cards live in UJusyncPathlineWidget; this widget
 * is the playback half of the deck.
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

	/** Reveals the deck with the eased glass animation. */
	void Open();

	/** Fades the deck out and collapses it once the tween finishes. */
	void Close();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Lets T/P toggle the panels even while focus sits inside one. */
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
	UButton* MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
		TObjectPtr<UTextBlock>* OutCaption = nullptr, float FontSize = 13.0f);
	UBorder* MakeRoundedBorder(const FName Name, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius, float OutlineWidth,
		const FMargin& InPadding = FMargin(0.0f));
	UButton* MakeSwitch(const FName Name, const FText& Tooltip, TObjectPtr<UBorder>& OutThumb);
	void SetSwitchState(UButton* Button, UBorder* Thumb, bool bOn);

	/** Pull count / index / sender time out of the spawner and repaint. */
	void Refresh();

	/**
	 * Plays the cached window at the transport's rate, one cached sample per
	 * step. Lives on the widget clock because the plugin's playback entry
	 * points would need a full rebuild, and this only re-applies samples that
	 * are already in the ring - it never requests or rebuilds a frame.
	 */
	void TickCachedPlayback(float DeltaSeconds);

	/**
	 * Starts the cached walk. bFromLiveEdge takes over from the frame that is
	 * already on screen (no cut, used by the sender-loop takeover); false cuts
	 * to the oldest cached frame first.
	 */
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

	/** Icon of PlayPauseButton; the glyph flips with the transport state. */
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

	/** Guards the programmatic SetValue calls. */
	bool bApplyingFromCode = false;
	bool bDraggingSlider = false;
	bool bLayoutBuilt = false;

	/** Eased show/hide; the panel owns it so T re-presses can reverse gently. */
	FJusyncPanelReveal Reveal;
	TArray<FJusyncGlassBlur> GlassBlurs;

	/** Transport playback state (drives the PLAY/PAUSE and LOOP controls). */
	bool bCachedPlaybackRunning = false;
	float CachedPlaybackAccumulator = 0.0f;

	/** Set when the user parks the transport on LIVE or PAUSE, so the sender
	 *  loop takeover does not immediately restart playback behind their back.
	 *  Pressing PLAY (or restarting PIE) clears it again. */
	bool bAutoLoopPlaybackSuppressed = false;

	float RefreshAccumulator = 0.0f;

	/** Wall clock of the previous NativeTick. A collapsed panel does not tick, so
	 *  a large gap means the HUD was closed (T) and any sender-loop latch it
	 *  reads afterwards is stale - it must not restart cached playback. */
	double LastWidgetTickSeconds = 0.0;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
