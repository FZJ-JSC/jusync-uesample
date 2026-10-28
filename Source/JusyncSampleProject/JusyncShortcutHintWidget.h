#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JusyncHUDPanelReveal.h"
#include "JusyncShortcutHintWidget.generated.h"

class UTextBlock;

UCLASS()
class UJusyncShortcutHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void EnsureLayoutBuilt();

	void RefreshKeys();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildLayout();
	void ApplyReveal();
	UTextBlock* MakeLabel(const FName Name, const FText& Label);
	UWidget* MakeRow(const FName BaseName, UTextBlock*& OutKey, const FText& Caption);

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlaybackKeyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PathlineKeyText;

	FJusyncPanelReveal Reveal;
	TArray<FJusyncGlassBlur> GlassBlurs;
	bool bIntroStarted = false;
	bool bLayoutBuilt = false;
};
