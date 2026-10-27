#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "JusyncPathlineWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class UVerticalBox;

/**
 * The right-hand LAYERS panel from the dimensional drawing: one 360 x 620
 * frame with a 60 px header and two 260 px cards. The PATHLINES card controls
 * the Niagara trail model; the POINT CLOUD card mirrors the spawner's point
 * properties. The geometry is fixed at the 1920x1080 reference; the panel is
 * anchored to the top-right at V1 = 24.
 *
 * As with UJusyncPlaybackWidget the tree is built in C++ (no .uasset), so it
 * works in Shipping. The card slots are placeholders in the drawing; the
 * controls here are the real pathline / point-cloud properties.
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
	void HandlePointsEnableClicked();

	UFUNCTION()
	void HandleHistoryChanged(float Value);

	UFUNCTION()
	void HandleWidthChanged(float Value);

	UFUNCTION()
	void HandlePointSizeChanged(float Value);

	UFUNCTION()
	void HandlePointGlowChanged(float Value);

	UFUNCTION()
	void HandleColorSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleHueSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleShapeSquareClicked();

	UFUNCTION()
	void HandleShapeCircleClicked();

	UFUNCTION()
	void HandleScalingSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleOrientationSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePointHueSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleNormalsClicked();

	UFUNCTION() void HandleMaxMinus() { StepMaxBy(-1); }
	UFUNCTION() void HandleMaxPlus() { StepMaxBy(+1); }
	UFUNCTION() void HandleExpiryMinus() { StepExpiryBy(-1); }
	UFUNCTION() void HandleExpiryPlus() { StepExpiryBy(+1); }
	UFUNCTION() void HandleQualityMinus() { StepQualityBy(-1); }
	UFUNCTION() void HandleQualityPlus() { StepQualityBy(+1); }

	/** The POINTS panel owns sizing: only the SCREEN option keeps the
	 *  fixed-screen-size override on. Every other scaling mode must be able to
	 *  change the component's ScalingMethod. */
	void SyncPointCloudFixedSizeMode();

	void StepMaxBy(int32 Direction);
	void StepExpiryBy(int32 Direction);
	void StepQualityBy(int32 Direction);

	void BuildLayout();
	UTextBlock* MakeLabel(const FName Name, const FText& Label);
	UButton* MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
		TObjectPtr<UTextBlock>* OutCaption = nullptr, float FontSize = 10.0f);
	UBorder* MakeRoundedBorder(const FName Name, const FLinearColor& Fill,
		const FLinearColor& Outline, float Radius, float OutlineWidth,
		const FMargin& InPadding = FMargin(0.0f));
	UButton* MakeSwitch(const FName Name, const FText& Tooltip, TObjectPtr<UBorder>& OutThumb);
	void SetSwitchState(UButton* Button, UBorder* Thumb, bool bOn);
	USlider* MakeSlider(const FName Name, float Min, float Max, float Step);
	UComboBoxString* MakeCombo(const FName Name, const TArray<FString>& Options, int32 DefaultIndex);
	void AddSliderRow(UVerticalBox* Parent, const FText& Label, const FName SliderName,
		float Min, float Max, float Step, TObjectPtr<USlider>& OutSlider,
		TObjectPtr<UTextBlock>& OutValue);
	void AddColumnLabel(UVerticalBox* Column, const FText& Label);
	UWidget* MakeStepper(const FName BaseName, float Width, TObjectPtr<UButton>& OutMinus,
		TObjectPtr<UButton>& OutPlus, TObjectPtr<UTextBlock>& OutValue);

	void Refresh();

	int32 EffectiveHistorySamples() const;
	int32 EffectiveMaxTracks() const;
	static int32 CurrentIntCvar(const TCHAR* Name, int32 Default);
	static void SetIntCvar(const TCHAR* Name, int32 Value);
	static float CurrentFloatCvar(const TCHAR* Name, float Default);
	static void SetFloatCvar(const TCHAR* Name, float Value);

	UPROPERTY(Transient)
	TObjectPtr<AJUSYNCFileSpawnerActor> Spawner;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathlineStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnableSwitch;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EnableThumb;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PointsSwitch;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PointsThumb;

	UPROPERTY(Transient)
	TObjectPtr<USlider> HistorySlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HistoryValueText;

	UPROPERTY(Transient)
	TObjectPtr<USlider> WidthSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WidthValueText;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ColorCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> HueCombo;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaxValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExpiryValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MaxMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MaxPlusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExpiryMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExpiryPlusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QualityMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QualityPlusButton;

	UPROPERTY(Transient)
	TObjectPtr<USlider> PointSizeSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointSizeValueText;

	/** jusync.Spawner.CloudEmissiveFloor: the cloud's self-glow. Owns both the
	 *  "too dark" and "washed out" endpoints, so it sits beside POINT SIZE. */
	UPROPERTY(Transient)
	TObjectPtr<USlider> PointGlowSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PointGlowValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShapeSquareButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ShapeCircleButton;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ScalingCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> OrientationCombo;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> PointHueCombo;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NormalsSwitch;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NormalsThumb;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QualityValueText;

	bool bApplyingFromCode = false;
	bool bLayoutBuilt = false;
	float RefreshAccumulator = 0.0f;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
