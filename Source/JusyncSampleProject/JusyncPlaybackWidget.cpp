#include "JusyncPlaybackWidget.h"

#include "JusyncPlaybackHUDModel.h"
#include "JusyncPlaybackHUDPlayerController.h"
#include "JUSYNCFileSpawnerActor.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "JusyncPlaybackWidget"

namespace
{
    /**
     * The visual language is the same dark glass as the rest of the runtime
     * HUD. Geometry comes from the dimensional drawing: V1 = 24, H7 = 72,
     * H8 = 44, R1 = 12, R2 = 7, R3 = 6, Z1 = 40.
     */
    namespace Theme
    {
        const FLinearColor PanelEdge(1.0f, 1.0f, 1.0f, 0.11f);
        const FLinearColor PanelFill(0.035f, 0.040f, 0.050f, 0.92f);
        const FLinearColor Divider(1.0f, 1.0f, 1.0f, 0.09f);

        const FLinearColor TextPrimary(0.94f, 0.95f, 0.97f, 1.0f);
        const FLinearColor TextMuted(0.60f, 0.64f, 0.70f, 1.0f);
        const FLinearColor TextShadow(0.0f, 0.0f, 0.0f, 0.65f);

        const FLinearColor Live(0.35f, 0.85f, 0.45f, 1.0f);
        const FLinearColor Paused(1.0f, 0.72f, 0.24f, 1.0f);
        const FLinearColor Error(0.95f, 0.38f, 0.34f, 1.0f);
        const FLinearColor Accent(0.28f, 0.62f, 1.00f, 1.0f);

        const FLinearColor ChipIdle(0.150f, 0.170f, 0.205f, 0.92f);
        const FLinearColor ChipHover(0.220f, 0.250f, 0.295f, 1.00f);
        const FLinearColor ChipPressed(0.200f, 0.450f, 0.850f, 1.00f);
        const FLinearColor ChipDisabled(0.100f, 0.110f, 0.130f, 0.45f);
        const FLinearColor WellFill(0.025f, 0.030f, 0.040f, 0.92f);
        const FLinearColor WellEdge(1.0f, 1.0f, 1.0f, 0.07f);
        const FLinearColor SliderTrack(1.0f, 1.0f, 1.0f, 0.18f);
        const FLinearColor SliderThumb(0.88f, 0.92f, 1.00f, 1.0f);

        constexpr float V1 = 24.0f;
        constexpr float H7 = 72.0f;
        constexpr float H8 = 44.0f;
        constexpr float H3 = 28.0f;
        constexpr float Z1 = 40.0f;
        constexpr float R1 = 12.0f;
        constexpr float R2 = 7.0f;
        constexpr float R3 = 6.0f;
        /** The transport is a centred deck, not an edge-to-edge dock. */
        constexpr float TransportWidth = 960.0f;

        constexpr float TitleSize = 11.0f;
        constexpr float ValueSize = 11.0f;
        constexpr float IconSize = 15.0f;
    }

    void StyleLabel(UTextBlock* Text, float Size, const FLinearColor& Color)
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FMath::RoundToInt(Size);
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetShadowColorAndOpacity(Theme::TextShadow);
        Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    }

    FButtonStyle MakeButtonStyle(const FLinearColor& Fill, const FLinearColor& Hover,
        const FLinearColor& Pressed, const FLinearColor& Disabled, float Radius)
    {
        FButtonStyle Style = FButtonStyle::GetDefault();
        Style.Normal = FSlateRoundedBoxBrush(Fill, Radius);
        Style.Hovered = FSlateRoundedBoxBrush(Hover, Radius);
        Style.Pressed = FSlateRoundedBoxBrush(Pressed, Radius);
        Style.Disabled = FSlateRoundedBoxBrush(Disabled, Radius);
        Style.NormalPadding = FMargin(0.0f);
        Style.PressedPadding = FMargin(0.0f);
        return Style;
    }

    FSliderStyle MakeSliderStyle()
    {
        FSliderStyle Style = FSliderStyle::GetDefault();
        const FSlateRoundedBoxBrush Track(Theme::SliderTrack, 3.0f, FLinearColor::Transparent, 0.0f,
            FVector2D(0.0f, 6.0f));
        const FSlateRoundedBoxBrush TrackHover(Theme::SliderTrack * 1.35f, 3.0f,
            FLinearColor::Transparent, 0.0f, FVector2D(0.0f, 6.0f));
        const FSlateRoundedBoxBrush TrackDisabled(Theme::SliderTrack * 0.5f, 3.0f,
            FLinearColor::Transparent, 0.0f, FVector2D(0.0f, 6.0f));
        const FSlateRoundedBoxBrush Thumb(Theme::SliderThumb, 7.0f,
            FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 1.0f, FVector2D(14.0f, 14.0f));
        const FSlateRoundedBoxBrush ThumbHover(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), 7.0f,
            FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 1.0f, FVector2D(14.0f, 14.0f));
        const FSlateRoundedBoxBrush ThumbDisabled(FLinearColor(0.55f, 0.58f, 0.62f, 0.5f), 7.0f,
            FLinearColor::Transparent, 0.0f, FVector2D(14.0f, 14.0f));

        Style.NormalBarImage = Track;
        Style.HoveredBarImage = TrackHover;
        Style.DisabledBarImage = TrackDisabled;
        Style.NormalThumbImage = Thumb;
        Style.HoveredThumbImage = ThumbHover;
        Style.DisabledThumbImage = ThumbDisabled;
        Style.BarThickness = 6.0f;
        return Style;
    }

    USizeBox* MakeSizeBox(UWidgetTree* Tree, UWidget* Child, float Width, float Height)
    {
        USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
        if (Width >= 0.0f)
        {
            Box->SetWidthOverride(Width);
        }
        if (Height >= 0.0f)
        {
            Box->SetHeightOverride(Height);
        }
        Box->AddChild(Child);
        return Box;
    }

    /**
     * Scrub position for a cached timeline index. The spawner treats any
     * position at or above 0.999 as "live edge" and leaves playback mode, so
     * the newest cached frame is requested just under that threshold and the
     * spawner rounds it back to the same index.
     */
    float ScrubPositionForIndex(int32 Index, int32 Count)
    {
        if (Count <= 1)
        {
            return 0.0f;
        }
        const int32 Last = Count - 1;
        return Index >= Last
            ? 0.9989f
            : static_cast<float>(Index) / static_cast<float>(Last);
    }
}

void UJusyncPlaybackWidget::BindSpawner(AJUSYNCFileSpawnerActor* InSpawner)
{
    Spawner = InSpawner;
    EnsureLayoutBuilt();
    Refresh();
}

namespace
{
    /**
     * Keep Tab and Shift+Tab from being eaten by Slate focus navigation while
     * a transport control is focused, so the panel widget can still receive
     * the toggle key and close the transport.
     */
    void DisableTabNavigation(UWidget* Widget)
    {
        if (!Widget)
        {
            return;
        }
        Widget->SetNavigationRuleBase(EUINavigation::Next, EUINavigationRule::Escape);
        Widget->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Escape);
    }

    /** Last window this panel pushed, so the idle guard in NativeTick does not
     *  write the console variable on every frame. */
    int32 GPlaybackMorphWindowMs = 0;

    /**
     * Playback morph handshake. jusync.Morph.CloudBlendMs is the window the
     * streamer morphs the displayed cloud over; the panel opens it while the
     * cached animation runs so consecutive frames glide into each other, and
     * closes it (0) for scrubs, single steps and LIVE, where a hard snapshot is
     * what the user asked for. 800/FPS leaves the tail of the frame for the
     * deferred cloud swap to land before the next frame starts.
     */
    void SetPlaybackMorphWindow(float FramesPerSecond)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                TEXT("jusync.Morph.CloudBlendMs")))
        {
            const float Fps = FMath::Clamp(FramesPerSecond, 1.0f, 240.0f);
            const int32 BlendMs = FMath::Clamp(FMath::RoundToInt(800.0f / Fps), 8, 4000);
            CVar->Set(BlendMs, ECVF_SetByConsole);
            GPlaybackMorphWindowMs = BlendMs;
        }
    }

    void ClearPlaybackMorphWindow()
    {
        GPlaybackMorphWindowMs = 0;
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(
                TEXT("jusync.Morph.CloudBlendMs")))
        {
            CVar->Set(0, ECVF_SetByConsole);
        }
    }
}

void UJusyncPlaybackWidget::EnsureLayoutBuilt()
{
    if (bLayoutBuilt)
    {
        return;
    }
    if (!WidgetTree)
    {
        return;
    }
    bLayoutBuilt = true;

    BuildLayout();

    for (UWidget* Control : {
             static_cast<UWidget*>(ScrubSlider.Get()),
             static_cast<UWidget*>(PrevButton.Get()),
             static_cast<UWidget*>(NextButton.Get()),
             static_cast<UWidget*>(PlayPauseButton.Get()),
             static_cast<UWidget*>(LiveButton.Get()),
             static_cast<UWidget*>(RateMinusButton.Get()),
             static_cast<UWidget*>(RatePlusButton.Get()),
             static_cast<UWidget*>(LoopSwitch.Get())})
    {
        DisableTabNavigation(Control);
    }

    if (ScrubSlider)
    {
        ScrubSlider->OnValueChanged.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderChanged);
        ScrubSlider->OnMouseCaptureBegin.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderCaptureBegin);
        ScrubSlider->OnMouseCaptureEnd.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderCaptureEnd);
    }
    if (PrevButton)
    {
        PrevButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandlePrevClicked);
    }
    if (NextButton)
    {
        NextButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleNextClicked);
    }
    if (PlayPauseButton)
    {
        PlayPauseButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandlePlayPauseClicked);
    }
    if (LiveButton)
    {
        LiveButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleLiveClicked);
    }
    if (RateMinusButton)
    {
        RateMinusButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleRateMinus);
    }
    if (RatePlusButton)
    {
        RatePlusButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleRatePlus);
    }
    if (LoopSwitch)
    {
        LoopSwitch->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleLoopClicked);
    }

    Refresh();
}

void UJusyncPlaybackWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureLayoutBuilt();
}

void UJusyncPlaybackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    TickCachedPlayback(InDeltaTime);

    // The morph window belongs to the running animation only: if playback was
    // stopped from anywhere we did not route through (panel closed, PIE ending,
    // frame count collapsing), restore exact per-frame snapshots for live data.
    if (!bCachedPlaybackRunning && GPlaybackMorphWindowMs != 0)
    {
        ClearPlaybackMorphWindow();
    }

    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator < RefreshIntervalSeconds)
    {
        return;
    }

    RefreshAccumulator = 0.0f;
    Refresh();
}

void UJusyncPlaybackWidget::TickCachedPlayback(float DeltaSeconds)
{
    if (!bCachedPlaybackRunning || !Spawner)
    {
        return;
    }

    const int32 Count = Spawner->GetCachedSampleCount();
    if (Count <= 1)
    {
        bCachedPlaybackRunning = false;
        ClearPlaybackMorphWindow();
        return;
    }

    const float FrameTime = 1.0f / FMath::Clamp(Spawner->TimeStepPlaybackFPS, 1.0f, 240.0f);
    int32 Index = FMath::Clamp(Spawner->GetPlaybackSampleIndex(), 0, Count - 1);
    bool bAdvanced = false;

    CachedPlaybackAccumulator += DeltaSeconds;
    while (CachedPlaybackAccumulator >= FrameTime)
    {
        CachedPlaybackAccumulator -= FrameTime;
        ++Index;
        if (Index >= Count)
        {
            if (Spawner->bLoopTimeStepAnimation)
            {
                Index = 0;
            }
            else
            {
                Index = Count - 1;
                bCachedPlaybackRunning = false;
                ClearPlaybackMorphWindow();
            }
        }
        bAdvanced = true;
        if (!bCachedPlaybackRunning)
        {
            break;
        }
    }

    if (bAdvanced)
    {
        Spawner->SetPlaybackPosition(ScrubPositionForIndex(Index, Count));
    }
}

FReply UJusyncPlaybackWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!InKeyEvent.IsRepeat())
    {
        if (AJusyncPlaybackHUDPlayerController* Controller =
            Cast<AJusyncPlaybackHUDPlayerController>(GetOwningPlayer()))
        {
            if (Controller->HandlePanelToggleKey(InKeyEvent.GetKey()))
            {
                return FReply::Handled();
            }
        }
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UJusyncPlaybackWidget::BuildLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // ---------------------------------------------------------------- HUD
    HUDPanel = MakeRoundedBorder(TEXT("HUDPanel"), Theme::PanelFill, Theme::PanelEdge,
        Theme::R1, 1.0f, FMargin(12.0f, 0.0f, 12.0f, 0.0f));
    HUDPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* HUDSlot = Root->AddChildToCanvas(HUDPanel);
    HUDSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    HUDSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    HUDSlot->SetPosition(FVector2D(Theme::V1, Theme::V1));
    HUDSlot->SetSize(FVector2D(320.0f, Theme::H8));

    UHorizontalBox* HUD = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("HUD"));
    HUDPanel->AddChild(HUD);

    HUDDot = MakeRoundedBorder(NAME_None, Theme::Live, FLinearColor::Transparent, 4.0f, 0.0f);
    USizeBox* DotBox = MakeSizeBox(WidgetTree, HUDDot, 8.0f, 8.0f);
    UHorizontalBoxSlot* DotSlot = HUD->AddChildToHorizontalBox(DotBox);
    DotSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
    DotSlot->SetVerticalAlignment(VAlign_Center);

    HUDStateText = MakeLabel(TEXT("HUDState"), FText::FromString(TEXT("LIVE")));
    StyleLabel(HUDStateText, Theme::ValueSize, Theme::Live);
    HUDStateText->SetJustification(ETextJustify::Left);
    // Keep the state label auto-sized: "CONNECTING" is wider than the old
    // fixed 68 px box and used to run into the FILES segment.
    UHorizontalBoxSlot* StateSlot = HUD->AddChildToHorizontalBox(HUDStateText);
    StateSlot->SetVerticalAlignment(VAlign_Center);

    auto AddHUDDivider = [this, HUD]()
    {
        UBorder* Line = MakeRoundedBorder(NAME_None, Theme::Divider,
            FLinearColor::Transparent, 0.0f, 0.0f);
        USizeBox* Box = MakeSizeBox(WidgetTree, Line, 1.0f, 20.0f);
        UHorizontalBoxSlot* Slot = HUD->AddChildToHorizontalBox(Box);
        Slot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 0.0f));
        Slot->SetVerticalAlignment(VAlign_Center);
    };

    AddHUDDivider();

    HUDFilesText = MakeLabel(TEXT("HUDFiles"), FText::FromString(TEXT("FILES --")));
    StyleLabel(HUDFilesText, Theme::ValueSize, Theme::TextPrimary);
    UHorizontalBoxSlot* FilesSlot = HUD->AddChildToHorizontalBox(HUDFilesText);
    FilesSlot->SetVerticalAlignment(VAlign_Center);

    AddHUDDivider();

    HUDTracksText = MakeLabel(TEXT("HUDTracks"), FText::FromString(TEXT("TRACKS 0")));
    StyleLabel(HUDTracksText, Theme::ValueSize, Theme::TextMuted);
    HUDTracksText->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* TracksSlot = HUD->AddChildToHorizontalBox(HUDTracksText);
    TracksSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TracksSlot->SetHorizontalAlignment(HAlign_Right);
    TracksSlot->SetVerticalAlignment(VAlign_Center);

    // ----------------------------------------------------------- TRANSPORT
    TransportBar = MakeRoundedBorder(TEXT("TransportBar"), Theme::PanelFill, Theme::PanelEdge,
        Theme::R1, 1.0f, FMargin(16.0f, 0.0f, 16.0f, 0.0f));
    UCanvasPanelSlot* BarSlot = Root->AddChildToCanvas(TransportBar);
    BarSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
    BarSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    BarSlot->SetPosition(FVector2D(0.0f, -Theme::V1));
    BarSlot->SetSize(FVector2D(Theme::TransportWidth, Theme::H7));

    UHorizontalBox* Bar = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("TransportRow"));
    TransportBar->AddChild(Bar);

    // Left: four 40 px transport buttons, 8 px apart.
    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("TransportButtons"));
    UHorizontalBoxSlot* ButtonsSlot = Bar->AddChildToHorizontalBox(Buttons);
    ButtonsSlot->SetVerticalAlignment(VAlign_Center);

    auto AddTransportButton = [this, Buttons](UButton* Button, bool bLast, float Width = Theme::Z1)
    {
        USizeBox* Box = MakeSizeBox(WidgetTree, Button, Width, Theme::Z1);
        UHorizontalBoxSlot* Slot = Buttons->AddChildToHorizontalBox(Box);
        Slot->SetPadding(FMargin(0.0f, 0.0f, bLast ? 0.0f : 8.0f, 0.0f));
        Slot->SetVerticalAlignment(VAlign_Center);
    };

    PrevButton = MakeButton(TEXT("PrevButton"), FText::FromString(TEXT("\u25C0")),
        LOCTEXT("PrevTip", "Step one cached sample backwards"), nullptr, Theme::IconSize);
    NextButton = MakeButton(TEXT("NextButton"), FText::FromString(TEXT("\u25B6")),
        LOCTEXT("NextTip", "Step one cached sample forwards"), nullptr, Theme::IconSize);
    PlayPauseButton = MakeButton(TEXT("PlayPauseButton"), FText::FromString(TEXT("||")),
        LOCTEXT("PlayPauseTip", "Play the cached samples at the current rate, or pause on the frame shown"),
        &PlayPauseCaption, Theme::IconSize);
    LiveButton = MakeButton(TEXT("LiveButton"), FText::FromString(TEXT("LIVE")),
        LOCTEXT("LiveTip", "Stop the playback and follow the live edge of the stream"),
        nullptr, 10.0f);

    AddTransportButton(PrevButton, false);
    AddTransportButton(PlayPauseButton, false);
    AddTransportButton(NextButton, false);
    AddTransportButton(LiveButton, true, 56.0f);

    // Middle: step label + sender time over the scrubber.
    UVerticalBox* Middle = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("TransportMiddle"));
    UHorizontalBoxSlot* MiddleSlot = Bar->AddChildToHorizontalBox(Middle);
    MiddleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    MiddleSlot->SetPadding(FMargin(24.0f, 0.0f, 24.0f, 0.0f));
    MiddleSlot->SetVerticalAlignment(VAlign_Center);

    UHorizontalBox* Meta = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("TransportMeta"));
    UVerticalBoxSlot* MetaSlot = Middle->AddChildToVerticalBox(Meta);
    MetaSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));

    StepText = MakeLabel(TEXT("StepText"), FText::FromString(TEXT("STEP --- / ---")));
    StyleLabel(StepText, Theme::ValueSize, Theme::TextPrimary);
    Meta->AddChildToHorizontalBox(StepText)->SetVerticalAlignment(VAlign_Center);

    TimeText = MakeLabel(TEXT("TimeText"), FText::FromString(TEXT("--")));
    StyleLabel(TimeText, Theme::ValueSize, Theme::TextMuted);
    TimeText->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* TimeSlot = Meta->AddChildToHorizontalBox(TimeText);
    TimeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TimeSlot->SetHorizontalAlignment(HAlign_Right);
    TimeSlot->SetVerticalAlignment(VAlign_Center);

    ScrubSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ScrubSlider"));
    ScrubSlider->SetMinValue(0.0f);
    ScrubSlider->SetMaxValue(1.0f);
    ScrubSlider->SetStepSize(0.0f);
    ScrubSlider->SetIndentHandle(false);
    ScrubSlider->SetWidgetStyle(MakeSliderStyle());
    USizeBox* ScrubBox = MakeSizeBox(WidgetTree, ScrubSlider, -1.0f, 16.0f);
    UVerticalBoxSlot* ScrubSlot = Middle->AddChildToVerticalBox(ScrubBox);
    ScrubSlot->SetHorizontalAlignment(HAlign_Fill);

    // Right: step counter, rate stepper, loop switch.
    UHorizontalBox* Right = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("TransportRight"));
    UHorizontalBoxSlot* RightSlot = Bar->AddChildToHorizontalBox(Right);
    RightSlot->SetVerticalAlignment(VAlign_Center);

    UBorder* CountWell = MakeRoundedBorder(NAME_None, Theme::WellFill, Theme::WellEdge,
        4.0f, 1.0f, FMargin(8.0f, 0.0f, 8.0f, 0.0f));
    StepCountText = MakeLabel(TEXT("StepCountText"), FText::FromString(TEXT("---/---")));
    StyleLabel(StepCountText, Theme::ValueSize, Theme::TextPrimary);
    StepCountText->SetJustification(ETextJustify::Center);
    CountWell->AddChild(StepCountText);
    USizeBox* CountBox = MakeSizeBox(WidgetTree, CountWell, 112.0f, Theme::H3);
    UHorizontalBoxSlot* CountSlot = Right->AddChildToHorizontalBox(CountBox);
    CountSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
    CountSlot->SetVerticalAlignment(VAlign_Center);

    UBorder* RateWell = MakeRoundedBorder(NAME_None, Theme::WellFill, Theme::WellEdge,
        4.0f, 1.0f);
    UHorizontalBox* RateRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("RateRow"));
    RateWell->AddChild(RateRow);

    RateMinusButton = MakeButton(TEXT("RateMinus"), FText::FromString(TEXT("-")),
        LOCTEXT("RateMinusTip", "Lower the time-step playback rate"), nullptr, 11.0f);
    RatePlusButton = MakeButton(TEXT("RatePlus"), FText::FromString(TEXT("+")),
        LOCTEXT("RatePlusTip", "Raise the time-step playback rate"), nullptr, 11.0f);
    RateMinusButton->SetStyle(MakeButtonStyle(FLinearColor::Transparent,
        FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
        FLinearColor::Transparent, 4.0f));
    RatePlusButton->SetStyle(MakeButtonStyle(FLinearColor::Transparent,
        FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
        FLinearColor::Transparent, 4.0f));

    USizeBox* RateMinusBox = MakeSizeBox(WidgetTree, RateMinusButton, 24.0f, Theme::H3);
    UHorizontalBoxSlot* RateMinusSlot = RateRow->AddChildToHorizontalBox(RateMinusBox);
    RateMinusSlot->SetVerticalAlignment(VAlign_Center);

    RateValueText = MakeLabel(TEXT("RateValue"), FText::FromString(TEXT("30")));
    StyleLabel(RateValueText, Theme::ValueSize, Theme::TextPrimary);
    RateValueText->SetJustification(ETextJustify::Center);
    UHorizontalBoxSlot* RateValueSlot = RateRow->AddChildToHorizontalBox(RateValueText);
    RateValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    RateValueSlot->SetHorizontalAlignment(HAlign_Center);
    RateValueSlot->SetVerticalAlignment(VAlign_Center);

    USizeBox* RatePlusBox = MakeSizeBox(WidgetTree, RatePlusButton, 24.0f, Theme::H3);
    UHorizontalBoxSlot* RatePlusSlot = RateRow->AddChildToHorizontalBox(RatePlusBox);
    RatePlusSlot->SetVerticalAlignment(VAlign_Center);

    USizeBox* RateBox = MakeSizeBox(WidgetTree, RateWell, 88.0f, Theme::H3);
    UHorizontalBoxSlot* RateSlot = Right->AddChildToHorizontalBox(RateBox);
    RateSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
    RateSlot->SetVerticalAlignment(VAlign_Center);

    UVerticalBox* LoopStack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("LoopStack"));
    LoopSwitch = MakeSwitch(TEXT("LoopSwitch"),
        LOCTEXT("LoopTip", "Loop the cached playback when it reaches the newest cached frame"),
        LoopThumb);
    USizeBox* LoopBox = MakeSizeBox(WidgetTree, LoopSwitch, 36.0f, 20.0f);
    UVerticalBoxSlot* LoopSlot = LoopStack->AddChildToVerticalBox(LoopBox);
    LoopSlot->SetHorizontalAlignment(HAlign_Center);

    UTextBlock* LoopLabel = MakeLabel(TEXT("LoopLabel"), FText::FromString(TEXT("LOOP")));
    StyleLabel(LoopLabel, 10.0f, Theme::TextMuted);
    LoopLabel->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* LoopLabelSlot = LoopStack->AddChildToVerticalBox(LoopLabel);
    LoopLabelSlot->SetHorizontalAlignment(HAlign_Center);
    LoopLabelSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

    UHorizontalBoxSlot* LoopStackSlot = Right->AddChildToHorizontalBox(LoopStack);
    LoopStackSlot->SetVerticalAlignment(VAlign_Center);

    Refresh();
}

UTextBlock* UJusyncPlaybackWidget::MakeLabel(const FName Name, const FText& Label)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(Label);
    return Text;
}

UButton* UJusyncPlaybackWidget::MakeButton(const FName Name, const FText& Label,
    const FText& Tooltip, TObjectPtr<UTextBlock>* OutCaption, float FontSize)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetToolTipText(Tooltip);
    Button->SetStyle(MakeButtonStyle(Theme::ChipIdle, Theme::ChipHover, Theme::ChipPressed,
        Theme::ChipDisabled, 8.0f));
    Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

    // NAME_None on purpose: ConstructWidget renames every widget into the
    // WidgetTree under its name, and a caption reusing the button's name would
    // try to replace the UButton with a UTextBlock.
    UTextBlock* Caption = MakeLabel(NAME_None, Label);
    StyleLabel(Caption, FontSize, Theme::TextPrimary);
    Caption->SetJustification(ETextJustify::Center);
    Button->AddChild(Caption);
    if (OutCaption)
    {
        *OutCaption = Caption;
    }

    if (UButtonSlot* CaptionSlot = Cast<UButtonSlot>(Caption->Slot))
    {
        CaptionSlot->SetPadding(FMargin(0.0f));
        CaptionSlot->SetHorizontalAlignment(HAlign_Center);
        CaptionSlot->SetVerticalAlignment(VAlign_Center);
    }
    return Button;
}

UBorder* UJusyncPlaybackWidget::MakeRoundedBorder(const FName Name, const FLinearColor& Fill,
    const FLinearColor& Outline, float Radius, float OutlineWidth, const FMargin& InPadding)
{
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
    Border->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
    Border->SetPadding(InPadding);
    return Border;
}

UButton* UJusyncPlaybackWidget::MakeSwitch(const FName Name, const FText& Tooltip,
    TObjectPtr<UBorder>& OutThumb)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetToolTipText(Tooltip);
    Button->SetStyle(MakeButtonStyle(FLinearColor::White, FLinearColor::White,
        FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), FLinearColor(0.3f, 0.3f, 0.3f, 0.5f), 10.0f));
    Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), NAME_None);
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Button->AddChild(Canvas);
    if (UButtonSlot* CanvasSlot = Cast<UButtonSlot>(Canvas->Slot))
    {
        CanvasSlot->SetPadding(FMargin(0.0f));
        CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
        CanvasSlot->SetVerticalAlignment(VAlign_Fill);
    }

    UBorder* Thumb = MakeRoundedBorder(NAME_None, FLinearColor::White,
        FLinearColor(0.0f, 0.0f, 0.0f, 0.12f), 7.0f, 1.0f);
    Thumb->SetVisibility(ESlateVisibility::HitTestInvisible);
    UCanvasPanelSlot* ThumbSlot = Canvas->AddChildToCanvas(Thumb);
    ThumbSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    ThumbSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    ThumbSlot->SetPosition(FVector2D(2.0f, 3.0f));
    ThumbSlot->SetSize(FVector2D(14.0f, 14.0f));

    OutThumb = Thumb;
    return Button;
}

void UJusyncPlaybackWidget::SetSwitchState(UButton* Button, UBorder* Thumb, bool bOn)
{
    if (Button)
    {
        Button->SetBackgroundColor(bOn ? Theme::Accent : Theme::ChipIdle);
    }
    if (Thumb)
    {
        if (UCanvasPanelSlot* ThumbSlot = Cast<UCanvasPanelSlot>(Thumb->Slot))
        {
            ThumbSlot->SetPosition(FVector2D(bOn ? 20.0f : 2.0f, 3.0f));
        }
    }
}

void UJusyncPlaybackWidget::Refresh()
{
    if (!TransportBar)
    {
        return;
    }

    const int32 Count = Spawner ? Spawner->GetCachedSampleCount() : 0;
    const int32 Index = Spawner
        ? FMath::Clamp(Spawner->GetPlaybackSampleIndex(), 0, FMath::Max(0, Count - 1))
        : 0;
    const bool bLive = Spawner && !Spawner->IsPlayingCachedSample();
    const bool bPlayingCached = bCachedPlaybackRunning;
    const double SampleTime = (Count > 0 && Spawner) ? Spawner->GetCachedSampleTime(Index) : -1.0;

    FString StateLabel = TEXT("OFFLINE");
    FLinearColor StateColor = Theme::TextMuted;
    if (Spawner)
    {
        if (Spawner->CurrentState == EJUSYNCSpawnerState::Error)
        {
            StateLabel = TEXT("ERROR");
            StateColor = Theme::Error;
        }
        else if (bPlayingCached)
        {
            StateLabel = TEXT("PLAYING");
            StateColor = Theme::Accent;
        }
        else if (Spawner->IsPlayingCachedSample())
        {
            StateLabel = TEXT("PAUSED");
            StateColor = Theme::Paused;
        }
        else if (Count > 0)
        {
            StateLabel = TEXT("LIVE");
            StateColor = Theme::Live;
        }
        else if (Spawner->CurrentState == EJUSYNCSpawnerState::Downloading
            || Spawner->CurrentState == EJUSYNCSpawnerState::Connecting
            || Spawner->CurrentState == EJUSYNCSpawnerState::FetchingList)
        {
            StateLabel = TEXT("CONNECTING");
            StateColor = Theme::TextMuted;
        }
        else
        {
            StateLabel = TEXT("WAITING");
            StateColor = Theme::TextMuted;
        }
    }

    if (HUDStateText)
    {
        HUDStateText->SetText(FText::FromString(StateLabel));
        HUDStateText->SetColorAndOpacity(FSlateColor(StateColor));
    }
    if (HUDDot)
    {
        HUDDot->SetBrushColor(StateColor);
    }
    if (HUDFilesText)
    {
        const int32 Total = Spawner ? Spawner->FilesTotal : 0;
        const int32 Done = Spawner ? Spawner->FilesDownloaded : 0;
        HUDFilesText->SetText(FText::FromString(Total > 0
            ? FString::Printf(TEXT("FILES %d/%d"), Done, Total)
            : TEXT("FILES --")));
    }
    if (HUDTracksText)
    {
        const int32 Tracks = Spawner ? Spawner->PathlineActiveTracks : 0;
        HUDTracksText->SetText(FText::FromString(FString::Printf(TEXT("TRACKS %d"), Tracks)));
        HUDTracksText->SetColorAndOpacity(FSlateColor(Tracks > 0 ? Theme::TextPrimary : Theme::TextMuted));
    }

    if (StepText)
    {
        StepText->SetText(FText::FromString(Count > 0
            ? FString::Printf(TEXT("STEP %03d / %03d"), Index + 1, Count)
            : TEXT("STEP --- / ---")));
    }
    if (StepCountText)
    {
        StepCountText->SetText(FText::FromString(Count > 0
            ? FString::Printf(TEXT("%03d/%03d"), Index + 1, Count)
            : TEXT("---/---")));
    }
    if (TimeText)
    {
        TimeText->SetText(FText::FromString(SampleTime >= 0.0
            ? FString::Printf(TEXT("%.2f S"), SampleTime)
            : TEXT("--")));
    }
    if (RateValueText)
    {
        const int32 Rate = Spawner
            ? FMath::Clamp(FMath::RoundToInt(Spawner->TimeStepPlaybackFPS), 1, 240)
            : 30;
        RateValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Rate)));
    }
    if (LoopSwitch)
    {
        SetSwitchState(LoopSwitch, LoopThumb, Spawner && Spawner->bLoopTimeStepAnimation);
        LoopSwitch->SetIsEnabled(Spawner != nullptr);
    }
    if (RateMinusButton)
    {
        RateMinusButton->SetIsEnabled(Spawner != nullptr);
    }
    if (RatePlusButton)
    {
        RatePlusButton->SetIsEnabled(Spawner != nullptr);
    }

    const bool bCanScrub = FJusyncPlaybackHUDModel::CanScrub(Count);
    if (ScrubSlider)
    {
        ScrubSlider->SetIsEnabled(bCanScrub);
        ScrubSlider->SetRenderOpacity(bCanScrub ? 1.0f : 0.45f);
    }
    if (PrevButton)
    {
        PrevButton->SetIsEnabled(Count > 1);
    }
    if (NextButton)
    {
        NextButton->SetIsEnabled(Count > 1);
    }
    if (LiveButton)
    {
        LiveButton->SetIsEnabled(Count > 0 && !bLive);
        LiveButton->SetColorAndOpacity(bLive || Count <= 0 ? FLinearColor::White : Theme::Paused);
    }
    if (PlayPauseButton)
    {
        const bool bCanFreeze = Spawner && Spawner->bEnableSampleCache && Count > 0;
        PlayPauseButton->SetIsEnabled(bCanFreeze);
        PlayPauseButton->SetRenderOpacity(bCanFreeze ? 1.0f : 0.45f);
        PlayPauseButton->SetColorAndOpacity(bPlayingCached ? Theme::Live : FLinearColor::White);
    }
    if (PlayPauseCaption)
    {
        PlayPauseCaption->SetText(FText::FromString(bPlayingCached ? TEXT("||") : TEXT("\u25B6")));
    }

    bApplyingFromCode = true;
    if (ScrubSlider && !bDraggingSlider)
    {
        ScrubSlider->SetValue(FJusyncPlaybackHUDModel::SampleIndexToScrubPosition(Index, Count));
    }
    bApplyingFromCode = false;
}

void UJusyncPlaybackWidget::HandleSliderChanged(float Value)
{
    if (bApplyingFromCode || !Spawner)
    {
        return;
    }

    // A drag is a scrub: snap to the exact cached frame, no glide.
    ClearPlaybackMorphWindow();
    Spawner->SetPlaybackPosition(Value);
    bCachedPlaybackRunning = false;
    CachedPlaybackAccumulator = 0.0f;
    Refresh();
}

void UJusyncPlaybackWidget::HandleSliderCaptureBegin()
{
    bDraggingSlider = true;
}

void UJusyncPlaybackWidget::HandleSliderCaptureEnd()
{
    bDraggingSlider = false;
}

void UJusyncPlaybackWidget::HandlePrevClicked()
{
    if (Spawner)
    {
        bCachedPlaybackRunning = false;
        CachedPlaybackAccumulator = 0.0f;
        ClearPlaybackMorphWindow();
        Spawner->PlaybackPreviousSample();
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleNextClicked()
{
    if (Spawner)
    {
        bCachedPlaybackRunning = false;
        CachedPlaybackAccumulator = 0.0f;
        ClearPlaybackMorphWindow();
        Spawner->PlaybackNextSample();
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleLiveClicked()
{
    if (Spawner)
    {
        bCachedPlaybackRunning = false;
        CachedPlaybackAccumulator = 0.0f;
        ClearPlaybackMorphWindow();
        Spawner->FollowLatestSample();
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandlePlayPauseClicked()
{
    if (!Spawner)
    {
        return;
    }

    if (bCachedPlaybackRunning)
    {
        bCachedPlaybackRunning = false;
        CachedPlaybackAccumulator = 0.0f;
        ClearPlaybackMorphWindow();
        Refresh();
        return;
    }

    const int32 Count = Spawner->GetCachedSampleCount();
    if (Count <= 1)
    {
        return;
    }

    // Following live, or parked on the newest frame, restarts at the oldest
    // cached sample; anywhere else the playback resumes from the frame shown.
    const int32 Index = FMath::Clamp(Spawner->GetPlaybackSampleIndex(), 0, Count - 1);
    if (!Spawner->IsPlayingCachedSample() || Index >= Count - 1)
    {
        // Jumping back to the oldest frame is a cut, not a transition.
        ClearPlaybackMorphWindow();
        Spawner->SetPlaybackPosition(0.0f);
    }

    // Frames advance on the morph window from here on: the cloud glides for
    // most of the frame and the deferred swap lands in the tail.
    SetPlaybackMorphWindow(Spawner->TimeStepPlaybackFPS);

    CachedPlaybackAccumulator = 0.0f;
    bCachedPlaybackRunning = true;
    Refresh();
}

void UJusyncPlaybackWidget::HandleRateMinus()
{
    if (Spawner)
    {
        const int32 Current = FMath::RoundToInt(Spawner->TimeStepPlaybackFPS);
        Spawner->TimeStepPlaybackFPS = static_cast<float>(FMath::Clamp(Current - 1, 1, 240));
        if (bCachedPlaybackRunning)
        {
            SetPlaybackMorphWindow(Spawner->TimeStepPlaybackFPS);
        }
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleRatePlus()
{
    if (Spawner)
    {
        const int32 Current = FMath::RoundToInt(Spawner->TimeStepPlaybackFPS);
        Spawner->TimeStepPlaybackFPS = static_cast<float>(FMath::Clamp(Current + 1, 1, 240));
        if (bCachedPlaybackRunning)
        {
            SetPlaybackMorphWindow(Spawner->TimeStepPlaybackFPS);
        }
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleLoopClicked()
{
    if (Spawner)
    {
        // The playback loop reads this flag on every step, so flipping it
        // takes effect at the next frame boundary.
        Spawner->bLoopTimeStepAnimation = !Spawner->bLoopTimeStepAnimation;
        Refresh();
    }
}

#undef LOCTEXT_NAMESPACE
