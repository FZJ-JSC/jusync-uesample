#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/Array.h"
#include "Types/SlateEnums.h"
#include "JusyncPathlineWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UTextBlock;

/**
 * Dedicated in-game control surface for pathlines, separate from the
 * transport panel: enable, per-sample colouring, colour (hue) filtering,
 * track-budget stepping and trail thickness.
 *
 * Same construction rules as UJusyncPlaybackWidget: layout built in code
 * (works in Shipping, no .uasset), state maths in FJusyncPathlineHUDModel.
 * MAX TRACKS and RADIUS write the matching jusync.Pathlines.* cvars because
 * those cvars intentionally override the spawner properties.
 */
UCLASS()
class UJusyncPathlineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void BindSpawner(AJUSYNCFileSpawnerActor* InSpawner);

	/** Idempotent; called from NativeOnInitialized and after binding. */
	void EnsureLayoutBuilt();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** Lets Tab/P toggle the panels even while focus sits inside one. */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleEnableClicked();

	UFUNCTION()
	void HandleColorClicked();

	UFUNCTION()
	void HandleHueToggle();

	UFUNCTION()
	void HandleToleranceMinus();

	UFUNCTION()
	void HandleTolerancePlus();

	/** Typed overrides: committed on Enter / focus-loss, clamped hard. */
	UFUNCTION()
	void HandleMaxInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleRadiusInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandlePointSizeInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleEmissiveInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION()
	void HandleSidesInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	// ± steppers beside the R / PT / EM / SIDES boxes. OnClicked is
	// dynamic-only in 5.8, so each needs its own UFUNCTION.
	UFUNCTION() void HandleRadiusMinus() { StepRadiusBy(-1); }
	UFUNCTION() void HandleRadiusPlus() { StepRadiusBy(+1); }
	UFUNCTION() void HandlePointSizeMinus() { StepPointSizeBy(-1); }
	UFUNCTION() void HandlePointSizePlus() { StepPointSizeBy(+1); }
	UFUNCTION() void HandleEmissiveMinus() { StepEmissiveBy(-1); }
	UFUNCTION() void HandleEmissivePlus() { StepEmissiveBy(+1); }
	UFUNCTION() void HandleSidesMinus() { StepSidesBy(-1); }
	UFUNCTION() void HandleSidesPlus() { StepSidesBy(+1); }

	void StepRadiusBy(int32 Direction);
	void StepPointSizeBy(int32 Direction);
	void StepEmissiveBy(int32 Direction);
	void StepSidesBy(int32 Direction);

	/** Hand keyboard back to the game after an Enter-commit, so P/Tab work again. */
	void ReleaseInputFocus();

	// OnClicked is dynamic-only in 5.8, so each chip needs its own UFUNCTION.
	UFUNCTION() void HandleHueChipRed() { ApplyHueChip(0); }
	UFUNCTION() void HandleHueChipYellow() { ApplyHueChip(1); }
	UFUNCTION() void HandleHueChipGreen() { ApplyHueChip(2); }
	UFUNCTION() void HandleHueChipCyan() { ApplyHueChip(3); }
	UFUNCTION() void HandleHueChipBlue() { ApplyHueChip(4); }
	UFUNCTION() void HandleHueChipMagenta() { ApplyHueChip(5); }

	/** Chip click: select that hue (or clear it when clicked again). */
	void ApplyHueChip(int32 ChipIndex);

	// ---- POINTS section: point-cloud visibility, glow and hue filter ----

	UFUNCTION()
	void HandlePointsEnableClicked();

	UFUNCTION()
	void HandlePointHueToggle();

	UFUNCTION()
	void HandlePointToleranceMinus();

	UFUNCTION()
	void HandlePointTolerancePlus();

	UFUNCTION()
	void HandlePointGlowInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	UFUNCTION() void HandlePointGlowMinus() { StepPointGlowBy(-1); }
	UFUNCTION() void HandlePointGlowPlus() { StepPointGlowBy(+1); }

	void StepPointGlowBy(int32 Direction);

	UFUNCTION() void HandlePointHueChipRed() { ApplyPointHueChip(0); }
	UFUNCTION() void HandlePointHueChipYellow() { ApplyPointHueChip(1); }
	UFUNCTION() void HandlePointHueChipGreen() { ApplyPointHueChip(2); }
	UFUNCTION() void HandlePointHueChipCyan() { ApplyPointHueChip(3); }
	UFUNCTION() void HandlePointHueChipBlue() { ApplyPointHueChip(4); }
	UFUNCTION() void HandlePointHueChipMagenta() { ApplyPointHueChip(5); }

	/** Point chip click: filter the clouds to that hue (again = clear). */
	void ApplyPointHueChip(int32 ChipIndex);

	void BuildLayout();
	UTextBlock* MakeLabel(const FName Name, const FText& Label);
	UButton* MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
		TObjectPtr<UTextBlock>* OutCaption = nullptr, bool bMini = false);
	/** Appends "[minus] [caption] [plus]" to a row. */
	void AddStepperWidgets(UHorizontalBox* Row, UButton* MinusButton, UTextBlock* Caption,
		UButton* PlusButton);

	void Refresh();

	/** Effective values respect the cvar-overrides-property precedence. */
	int32 EffectiveMaxTracks() const;
	float EffectiveRadius() const;
	static float CurrentFloatCvar(const TCHAR* Name, float Default);
	static void SetFloatCvar(const TCHAR* Name, float Value);

	UPROPERTY(Transient)
	TObjectPtr<AJUSYNCFileSpawnerActor> Spawner;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnableButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnableCaption;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ColorButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ColorCaption;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HueButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HueCaption;

	/** One chip per hue in FJusyncPathlineHUDModel::HueChipAngles(). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> HueChipButtons;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ToleranceMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> TolerancePlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToleranceCaption;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> MaxInput;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> RadiusInput;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> PointSizeInput;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> EmissiveInput;
	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> SidesInput;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RadiusMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> RadiusPlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointSizeMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointSizePlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> EmissiveMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> EmissivePlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SidesMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> SidesPlusButton;

	// POINTS section widgets.
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointsEnableButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointsEnableCaption;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PointHueButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointHueCaption;

	/** One chip per hue in FJusyncPathlineHUDModel::HueChipAngles(). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> PointHueChipButtons;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PointToleranceMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointTolerancePlusButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointToleranceCaption;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> PointGlowInput;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointGlowMinusButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PointGlowPlusButton;

	bool bLayoutBuilt = false;
	float RefreshAccumulator = 0.0f;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
