#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "JusyncHUDPanelReveal.h"
#include "JusyncPathlineWidget.generated.h"

class AJUSYNCFileSpawnerActor;
class UBorder;
class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class UVerticalBox;

UCLASS()
class UJusyncPathlineWidget : public UUserWidget
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
	void HandleEnableClicked();

	UFUNCTION()
	void HandlePointsEnableClicked();

	UFUNCTION()
	void HandleHistoryChanged(float Value);

	UFUNCTION()
	void HandleWidthChanged(float Value);

	UFUNCTION()
	void HandlePathlineGlowChanged(float Value);

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

	void SyncPointCloudFixedSizeMode();

	void StepMaxBy(int32 Direction);
	void StepExpiryBy(int32 Direction);
	void StepQualityBy(int32 Direction);

	void BuildLayout();
	void ApplyReveal();
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
	TObjectPtr<USlider> PathlineGlowSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathlineGlowValueText;

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

	FJusyncPanelReveal Reveal;
	TArray<FJusyncGlassBlur> GlassBlurs;

	static constexpr float RefreshIntervalSeconds = 0.1f;
};
