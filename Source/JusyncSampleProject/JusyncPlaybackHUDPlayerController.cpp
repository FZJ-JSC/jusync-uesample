#include "JusyncPlaybackHUDPlayerController.h"

#include "JusyncPlaybackWidget.h"
#include "JusyncPathlineWidget.h"
#include "JusyncShortcutHintWidget.h"
#include "JUSYNCFileSpawnerActor.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

namespace
{
    void ReleaseFocusIfInside(const UUserWidget* Widget)
    {
        if (Widget && FSlateApplication::IsInitialized()
            && Widget->HasFocusedDescendants())
        {
            FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
        }
    }
}

void AJusyncPlaybackHUDPlayerController::TogglePanel()
{
	if (!PlaybackWidget)
	{
		return;
	}

	bPanelVisible = !bPanelVisible;

	if (!bPanelVisible)
	{
		ReleaseFocusIfInside(PlaybackWidget);
	}
	if (bPanelVisible)
	{
		PlaybackWidget->Open();
	}
	else
	{
		PlaybackWidget->Close();
	}
	ApplyPanelInputMode();

	UE_LOG(LogTemp, Log, TEXT("JUSYNC playback HUD %s (toggle key %s)"),
		bPanelVisible ? TEXT("shown") : TEXT("hidden"), *ToggleKey.ToString());
}

void AJusyncPlaybackHUDPlayerController::TogglePathlinePanel()
{
	if (!PathlineWidget)
	{
		return;
	}

	bPathlinePanelVisible = !bPathlinePanelVisible;

	if (!bPathlinePanelVisible)
	{
		ReleaseFocusIfInside(PathlineWidget);
	}
	if (bPathlinePanelVisible)
	{
		PathlineWidget->Open();
	}
	else
	{
		PathlineWidget->Close();
	}
	ApplyPanelInputMode();

	UE_LOG(LogTemp, Log, TEXT("JUSYNC pathline panel %s (toggle key %s)"),
		bPathlinePanelVisible ? TEXT("shown") : TEXT("hidden"), *PathlineToggleKey.ToString());
}

void AJusyncPlaybackHUDPlayerController::ApplyPanelInputMode()
{
	const bool bAnyVisible = bPanelVisible || bPathlinePanelVisible;
	SetShowMouseCursor(bAnyVisible);

	if (bAnyVisible)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AJusyncPlaybackHUDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AJUSYNCFileSpawnerActor> It(GetWorld()); It; ++It)
	{
		Spawner = *It;
		break;
	}

	if (!Spawner)
	{
		UE_LOG(LogTemp, Log, TEXT("JUSYNC playback HUD disabled: no AJUSYNCFileSpawnerActor in %s"),
			*GetWorld()->GetMapName());
		return;
	}

	const TSubclassOf<UJusyncPlaybackWidget> WidgetClass = PlaybackWidgetClass
		? PlaybackWidgetClass
		: TSubclassOf<UJusyncPlaybackWidget>(UJusyncPlaybackWidget::StaticClass());

	PlaybackWidget = CreateWidget<UJusyncPlaybackWidget>(this, WidgetClass);
	if (!PlaybackWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("JUSYNC playback HUD: widget creation failed"));
		return;
	}

	PlaybackWidget->AddToViewport(101);
	PlaybackWidget->SetVisibility(ESlateVisibility::Collapsed);
	PlaybackWidget->BindSpawner(Spawner);

	const TSubclassOf<UJusyncPathlineWidget> PathlineClass = PathlineWidgetClass
		? PathlineWidgetClass
		: TSubclassOf<UJusyncPathlineWidget>(UJusyncPathlineWidget::StaticClass());

	PathlineWidget = CreateWidget<UJusyncPathlineWidget>(this, PathlineClass);
	if (PathlineWidget)
	{
		PathlineWidget->AddToViewport(100);
		PathlineWidget->SetVisibility(ESlateVisibility::Collapsed);
		PathlineWidget->BindSpawner(Spawner);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("JUSYNC pathline panel: widget creation failed"));
	}

	const TSubclassOf<UJusyncShortcutHintWidget> HintClass = ShortcutHintClass
		? ShortcutHintClass
		: TSubclassOf<UJusyncShortcutHintWidget>(UJusyncShortcutHintWidget::StaticClass());

	ShortcutHintWidget = CreateWidget<UJusyncShortcutHintWidget>(this, HintClass);
	if (ShortcutHintWidget)
	{
		ShortcutHintWidget->EnsureLayoutBuilt();
		ShortcutHintWidget->RefreshKeys();
		ShortcutHintWidget->AddToViewport(99);
		UE_LOG(LogTemp, Log, TEXT("JUSYNC shortcut legend ready (%s playback, %s pathlines)"),
			*ToggleKey.ToString(), *PathlineToggleKey.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("JUSYNC shortcut legend: widget creation failed"));
	}

	if (FSlateApplication::IsInitialized())
	{
		PreInputKeyHandle = FSlateApplication::Get()
			.OnApplicationPreInputKeyDownListener()
			.AddUObject(this, &AJusyncPlaybackHUDPlayerController::HandlePreInputKey);
	}
}

void AJusyncPlaybackHUDPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FSlateApplication::IsInitialized() && PreInputKeyHandle.IsValid())
	{
		FSlateApplication::Get()
			.OnApplicationPreInputKeyDownListener()
			.Remove(PreInputKeyHandle);
		PreInputKeyHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

bool AJusyncPlaybackHUDPlayerController::IsGameViewportFocused() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UGameViewportClient* GameViewport = World->GetGameViewport();
	if (!GameViewport)
	{
		return false;
	}

	const TSharedPtr<SViewport> ViewportWidget = GameViewport->GetGameViewportWidget();
	return ViewportWidget.IsValid()
		&& (ViewportWidget->HasKeyboardFocus() || ViewportWidget->HasFocusedDescendants());
}

void AJusyncPlaybackHUDPlayerController::HandlePreInputKey(const FKeyEvent& KeyEvent)
{
	if (KeyEvent.IsRepeat() || !IsGameViewportFocused())
	{
		return;
	}

	HandlePanelToggleKey(KeyEvent.GetKey(), true);
}

bool AJusyncPlaybackHUDPlayerController::HandlePanelToggleKey(const FKey& Key, bool bFromPreInput)
{
	if (Key != ToggleKey && Key != PathlineToggleKey)
	{
		return false;
	}

	if (!bFromPreInput
		&& Key == PreInputHandledKey
		&& GFrameCounter == PreInputHandledFrame)
	{
		return true;
	}

	if (bFromPreInput)
	{
		PreInputHandledKey = Key;
		PreInputHandledFrame = GFrameCounter;
	}

	if (Key == ToggleKey)
	{
		TogglePanel();
	}
	else
	{
		TogglePathlinePanel();
	}
	return true;
}

bool AJusyncPlaybackHUDPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed && HandlePanelToggleKey(Params.Key))
	{
		return true;
	}

	return Super::InputKey(Params);
}

AJusyncPlaybackHUDPlayerController::AJusyncPlaybackHUDPlayerController()
{
}
