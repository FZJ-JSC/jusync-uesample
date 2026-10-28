#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JusyncHUDPanelReveal.h"
#include "JusyncShortcutHintWidget.generated.h"

class UTextBlock;

/**
 * Small persistent key legend that sits under the LIVE pill from the moment
 * the level starts: T opens the playback transport, P opens the pathlines
 * panel. It is deliberately not part of either panel, because those two are
 * collapsed most of the time and the legend has to survive their toggling.
 *
 * The card reuses the runtime HUD glass (background blur + smoked rounded
 * border) and is SelfHitTestInvisible, so it never steals the mouse from the
 * scene or from the panels behind it. The key names are read from the
 * controller, so changing ToggleKey / PathlineToggleKey moves the legend too.
 */
UCLASS()
class UJusyncShortcutHintWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Idempotent; called from NativeOnInitialized. */
	void EnsureLayoutBuilt();

	/** Re-reads the owning controller's keys and re-labels the keycaps. */
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

	/** Gentle level-start intro; the card is the first HUD surface in view. */
	FJusyncPanelReveal Reveal;
	TArray<FJusyncGlassBlur> GlassBlurs;
	bool bIntroStarted = false;
	bool bLayoutBuilt = false;
};
