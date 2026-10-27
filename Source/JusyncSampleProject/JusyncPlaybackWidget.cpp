#include "JusyncPlaybackWidget.h"

#include "JusyncPlaybackHUDModel.h"
#include "JusyncPlaybackHUDPlayerController.h"
#include "JUSYNCFileSpawnerActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "JusyncPlaybackWidget"

namespace
{
    /**
     * The whole look lives here so it can be retuned in one place. Colors are
     * multiplied against the engine's own widget art (no .uasset brushes), which
     * is what keeps this panel working in Shipping builds.
     */
    namespace Theme
    {
        // Card
        const FLinearColor PanelEdge(1.0f, 1.0f, 1.0f, 0.10f);
        const FLinearColor PanelFill(0.035f, 0.040f, 0.050f, 0.88f);
        const FLinearColor Divider(1.0f, 1.0f, 1.0f, 0.09f);

        // Type
        const FLinearColor TextPrimary(0.94f, 0.95f, 0.97f, 1.0f);
        const FLinearColor TextMuted(0.60f, 0.64f, 0.70f, 1.0f);
        const FLinearColor TextShadow(0.0f, 0.0f, 0.0f, 0.65f);

        // State accents
        const FLinearColor Live(0.35f, 0.85f, 0.45f, 1.0f);
        const FLinearColor Paused(1.0f, 0.72f, 0.24f, 1.0f);
        const FLinearColor Accent(0.28f, 0.62f, 1.00f, 1.0f);

        // Controls. These tint the engine's own button art, so they must be
        // near-opaque: UButton multiplies BackgroundColor into the brush, and a
        // translucent brush tint would make every state read as almost invisible.
        const FLinearColor ChipIdle(0.150f, 0.170f, 0.205f, 0.92f);
        const FLinearColor ChipHover(0.220f, 0.250f, 0.295f, 1.00f);
        const FLinearColor ChipPressed(0.200f, 0.450f, 0.850f, 1.00f);
        const FLinearColor ChipDisabled(0.100f, 0.110f, 0.130f, 0.45f);
        const FLinearColor SliderTrack(1.0f, 1.0f, 1.0f, 0.20f);

        // Metrics
        constexpr float PanelWidth = 640.0f;
        constexpr float TitleSize = 11.0f;
        constexpr float StatusSize = 17.0f;
        constexpr float ChipSize = 13.0f;
        constexpr float HintSize = 10.0f;
        const FMargin ChipPadding(14.0f, 5.0f);
    }

    const TCHAR* LiveOptionLabel = TEXT("LIVE");

    void StyleLabel(UTextBlock* Text, float Size, const FLinearColor& Color)
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FMath::RoundToInt(Size);
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        // The panel sits over lit warehouse scenes, so the glyphs need an edge.
        Text->SetShadowColorAndOpacity(Theme::TextShadow);
        Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
    }

    /** Engine button art, retinted per state; no asset dependency. */
    FButtonStyle MakeChipStyle()
    {
        FButtonStyle Style = FButtonStyle::GetDefault();
        Style.Normal.TintColor = FSlateColor(Theme::ChipIdle);
        Style.Hovered.TintColor = FSlateColor(Theme::ChipHover);
        Style.Pressed.TintColor = FSlateColor(Theme::ChipPressed);
        Style.Disabled.TintColor = FSlateColor(Theme::ChipDisabled);
        return Style;
    }

    /**
     * Chip geometry comes from UButtonSlot padding, not a SizeBox: 5.8's
     * SetWidthOverride(0) turns the override ON with zero, which collapses
     * whatever it wraps, so frames are only used where a hard size is wanted.
     */
}

void UJusyncPlaybackWidget::BindSpawner(AJUSYNCFileSpawnerActor* InSpawner)
{
    Spawner = InSpawner;
    LastTimelineSignature.Reset();
    // The controller may bind before AddToViewport fired Initialize(), so make
    // sure the tree exists before Refresh() tries to paint it.
    EnsureLayoutBuilt();
    Refresh();
}

void UJusyncPlaybackWidget::EnsureLayoutBuilt()
{
    if (bLayoutBuilt)
    {
        return;
    }
    if (!WidgetTree)
    {
        // BindSpawner ran before AddToViewport/Initialize(); NativeOnInitialized
        // will call back through here once the tree exists.
        return;
    }
    bLayoutBuilt = true;

    BuildLayout();

    ScrubSlider->OnValueChanged.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderChanged);
    ScrubSlider->OnMouseCaptureBegin.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderCaptureBegin);
    ScrubSlider->OnMouseCaptureEnd.AddDynamic(this, &UJusyncPlaybackWidget::HandleSliderCaptureEnd);
    PrevButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandlePrevClicked);
    NextButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleNextClicked);
    PlayPauseButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandlePlayPauseClicked);
    LiveButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandleLiveClicked);
    PathlineButton->OnClicked.AddDynamic(this, &UJusyncPlaybackWidget::HandlePathlineClicked);
    TimestepCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPlaybackWidget::HandleTimestepSelected);

    // Nothing to paint until a spawner is attached, but start in the empty
    // state so the panel never shows a stale frame.
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

    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator < RefreshIntervalSeconds)
    {
        return;
    }

    RefreshAccumulator = 0.0f;
    Refresh();
}

FReply UJusyncPlaybackWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Slate only routes keys along the focused widget's path, so once the user
    // clicks this panel the player controller's InputKey never sees Tab/P any
    // more. Catch them here (before Tab becomes focus traversal) and hand
    // them to the shared toggle handler.
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

    // Outer border is only a hairline edge; the card itself is the inner one.
    UBorder* Edge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
    Edge->SetBrushColor(Theme::PanelEdge);
    Edge->SetPadding(FMargin(1.0f));

    UCanvasPanelSlot* EdgeSlot = Root->AddChildToCanvas(Edge);
    EdgeSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
    EdgeSlot->SetAlignment(FVector2D(0.5f, 1.0f));
    EdgeSlot->SetPosition(FVector2D(0.0f, -20.0f));
    EdgeSlot->SetAutoSize(true);

    Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(Theme::PanelFill);
    Panel->SetPadding(FMargin(16.0f, 12.0f, 16.0f, 12.0f));
    Edge->AddChild(Panel);

    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
    Width->SetWidthOverride(Theme::PanelWidth);
    Panel->AddChild(Width);

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("Stack"));
    Width->AddChild(Stack);

    // Row 1: what this panel is, and how to get rid of it.
    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), NAME_None);
    Stack->AddChildToVerticalBox(Header);

    UTextBlock* Title = MakeLabel(NAME_None, LOCTEXT("PanelTitle", "JUSYNC PLAYBACK"));
    StyleLabel(Title, Theme::TitleSize, Theme::TextMuted);
    Header->AddChildToHorizontalBox(Title);

    UTextBlock* Hint = MakeLabel(NAME_None, LOCTEXT("PanelHint", "TAB hides  ·  drag to scrub"));
    StyleLabel(Hint, Theme::HintSize, Theme::TextMuted);
    UHorizontalBoxSlot* HintSlot = Header->AddChildToHorizontalBox(Hint);
    HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    HintSlot->SetHorizontalAlignment(HAlign_Right);

    UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
    Rule->SetBrushColor(Theme::Divider);
    USizeBox* RuleHeight = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
    RuleHeight->SetHeightOverride(1.0f);
    RuleHeight->AddChild(Rule);
    UVerticalBoxSlot* RuleSlot = Stack->AddChildToVerticalBox(RuleHeight);
    RuleSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 8.0f));

    // Row 2: the readout, e.g. "t=12.5   [4 / 8]".
    StatusText = MakeLabel(TEXT("StatusText"), FText::GetEmpty());
    StyleLabel(StatusText, Theme::StatusSize, Theme::TextPrimary);
    UVerticalBoxSlot* StatusSlot = Stack->AddChildToVerticalBox(StatusText);
    StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    StatusSlot->SetHorizontalAlignment(HAlign_Fill);

    // Row 3: the scrub bar over the cached samples.
    ScrubSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("ScrubSlider"));
    ScrubSlider->SetMinValue(0.0f);
    ScrubSlider->SetMaxValue(1.0f);
    ScrubSlider->SetStepSize(0.0f);
    ScrubSlider->SetIndentHandle(false);
    ScrubSlider->SetSliderBarColor(Theme::SliderTrack);
    ScrubSlider->SetSliderHandleColor(Theme::Accent);
    UVerticalBoxSlot* SliderSlot = Stack->AddChildToVerticalBox(ScrubSlider);
    SliderSlot->SetHorizontalAlignment(HAlign_Fill);
    SliderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));

    // Row 4: transport plus the timestep picker.
    UHorizontalBox* Transport = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("Transport"));
    Stack->AddChildToVerticalBox(Transport);

    PrevButton = MakeButton(TEXT("PrevButton"), FText::FromString(TEXT("<")),
        LOCTEXT("PrevTip", "Step one cached sample backwards"));
    NextButton = MakeButton(TEXT("NextButton"), FText::FromString(TEXT(">")),
        LOCTEXT("NextTip", "Step one cached sample forwards"));
    PlayPauseButton = MakeButton(TEXT("PlayPauseButton"),
        FJusyncPlaybackHUDModel::MakePlayPauseLabel(false),
        LOCTEXT("PlayPauseTip", "Freeze the stream on the newest frame, or resume following it"),
        &PlayPauseCaption);
    LiveButton = MakeButton(TEXT("LiveButton"), FText::FromString(TEXT("LIVE")),
        LOCTEXT("LiveTip", "Resume the live edge of the stream"));
    PathlineButton = MakeButton(TEXT("PathlineButton"),
        FJusyncPlaybackHUDModel::MakePathlineLabel(false),
        LOCTEXT("PathlineTip", "Show or hide the particle trail tubes (keeps the frame budget free when off)"),
        &PathlineCaption);

    auto AddChip = [Transport](UButton* Chip)
    {
        UHorizontalBoxSlot* ChipSlot = Transport->AddChildToHorizontalBox(Chip);
        ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
        ChipSlot->SetVerticalAlignment(VAlign_Center);
    };

    AddChip(PrevButton);
    AddChip(NextButton);
    AddChip(PlayPauseButton);
    AddChip(LiveButton);
    AddChip(PathlineButton);

    TimestepCombo = WidgetTree->ConstructWidget<UComboBoxString>(
        UComboBoxString::StaticClass(), TEXT("TimestepCombo"));
    FComboBoxStyle ComboStyle = TimestepCombo->GetWidgetStyle();
    ComboStyle.ContentPadding = FMargin(10.0f, 5.0f);
    TimestepCombo->SetWidgetStyle(ComboStyle);

    UHorizontalBoxSlot* ComboSlot = Transport->AddChildToHorizontalBox(TimestepCombo);
    ComboSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
    ComboSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ComboSlot->SetHorizontalAlignment(HAlign_Fill);
}

UTextBlock* UJusyncPlaybackWidget::MakeLabel(const FName Name, const FText& Label)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(Label);
    return Text;
}

UButton* UJusyncPlaybackWidget::MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
    TObjectPtr<UTextBlock>* OutCaption)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetToolTipText(Tooltip);
    Button->SetStyle(MakeChipStyle());
    Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

    // NAME_None on purpose: ConstructWidget renames every widget into the
    // WidgetTree under its name, and a caption reusing the button's name would
    // try to replace the UButton with a UTextBlock - a fatal "Cannot replace
    // existing object of a different class" at widget creation. Letting the
    // tree generate the caption's name keeps siblings unique.
    UTextBlock* Caption = MakeLabel(NAME_None, Label);
    StyleLabel(Caption, Theme::ChipSize, Theme::TextPrimary);
    Caption->SetJustification(ETextJustify::Center);
    Button->AddChild(Caption);
    if (OutCaption)
    {
        *OutCaption = Caption;
    }

    // UButton itself has no padding or alignment in 5.8; the content slot does.
    if (UButtonSlot* CaptionSlot = Cast<UButtonSlot>(Caption->Slot))
    {
        CaptionSlot->SetPadding(Theme::ChipPadding);
        CaptionSlot->SetHorizontalAlignment(HAlign_Center);
        CaptionSlot->SetVerticalAlignment(VAlign_Center);
    }
    return Button;
}

void UJusyncPlaybackWidget::Refresh()
{
    if (!Panel)
    {
        return;
    }

    const int32 Count = Spawner ? Spawner->GetCachedSampleCount() : 0;
    const int32 Index = Spawner ? FMath::Clamp(Spawner->GetPlaybackSampleIndex(), 0, FMath::Max(0, Count - 1)) : 0;
    const bool bIsLive = Spawner && !Spawner->IsPlayingCachedSample();
    const double SampleTime = (Count > 0 && Spawner)
        ? Spawner->GetCachedSampleTime(Index)
        : -1.0;

    StatusText->SetText(FJusyncPlaybackHUDModel::MakeStatusText(Index, Count, SampleTime, bIsLive));

    // The readout carries the state at a glance: green following the stream,
    // amber parked in history, grey when nothing has arrived yet.
    const FLinearColor StatusColor = Count <= 0
        ? Theme::TextMuted
        : (bIsLive ? Theme::Live : Theme::Paused);
    StatusText->SetColorAndOpacity(FSlateColor(StatusColor));

    const bool bCanScrub = FJusyncPlaybackHUDModel::CanScrub(Count);
    ScrubSlider->SetIsEnabled(bCanScrub);
    // A disabled slider has no distinct art of its own, so dim it explicitly.
    ScrubSlider->SetRenderOpacity(bCanScrub ? 1.0f : 0.45f);

    PrevButton->SetIsEnabled(Count > 0);
    NextButton->SetIsEnabled(Count > 0);
    LiveButton->SetIsEnabled(Count > 0 && !bIsLive);
    // UButton multiplies ColorAndOpacity into its content, so this is the
    // cheapest way to make "click me to get back to the live edge" legible:
    // the caption goes amber while parked, neutral while already following.
    LiveButton->SetColorAndOpacity(bIsLive || Count <= 0 ? FLinearColor::White : Theme::Paused);

    // Freeze needs the sample cache: below the spawner's 0.999 live-edge
    // threshold the visible actor is parked, at or above it the stream keeps
    // applying, so with no cache there is nothing to freeze.
    const bool bCanFreeze = Spawner && Spawner->bEnableSampleCache && Count > 0;
    PlayPauseButton->SetIsEnabled(bCanFreeze);
    PlayPauseButton->SetRenderOpacity(bCanFreeze ? 1.0f : 0.45f);
    PlayPauseCaption->SetText(FJusyncPlaybackHUDModel::MakePlayPauseLabel(bIsLive));
    PlayPauseButton->SetColorAndOpacity(bIsLive ? FLinearColor::White : Theme::Paused);

    // The trail toggle writes straight through to the spawner property that
    // SyncPathlineManager pushes to the manager every tick, so the state is
    // whatever the spawner says - Details panel edits included.
    const bool bTrailsOn = Spawner && Spawner->bEnablePathlines;
    PathlineButton->SetIsEnabled(Spawner != nullptr);
    PathlineCaption->SetText(FJusyncPlaybackHUDModel::MakePathlineLabel(bTrailsOn));
    PathlineButton->SetColorAndOpacity(bTrailsOn ? Theme::Accent : Theme::TextMuted);

    SyncTimestepOptions(Count);

    bApplyingFromCode = true;
    if (!bDraggingSlider)
    {
        ScrubSlider->SetValue(FJusyncPlaybackHUDModel::SampleIndexToScrubPosition(Index, Count));
    }

    TimestepCombo->SetSelectedIndex(
        FJusyncPlaybackHUDModel::SampleIndexToComboSelection(Index, Count, bIsLive));
    bApplyingFromCode = false;
}

void UJusyncPlaybackWidget::SyncTimestepOptions(int32 SampleCount)
{
    // Rebuilding the options closes an open dropdown, so only do it when the
    // cached timeline really moved (a new sample arrived, or the cache was
    // cleared). Sender times identify the timeline; index alone does not,
    // because the ring shifts older samples down as new ones arrive.
    FString Signature = FString::Printf(TEXT("%d"), SampleCount);
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        Signature += FString::Printf(TEXT("|%.4f"), Spawner ? Spawner->GetCachedSampleTime(Index) : -1.0);
    }

    if (Signature == LastTimelineSignature)
    {
        return;
    }

    LastTimelineSignature = Signature;

    bApplyingFromCode = true;
    TimestepCombo->ClearOptions();
    TimestepCombo->AddOption(MakeLiveOptionLabel());
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        TimestepCombo->AddOption(FJusyncPlaybackHUDModel::MakeTimestepLabel(
            Index, SampleCount, Spawner ? Spawner->GetCachedSampleTime(Index) : -1.0));
    }
    bApplyingFromCode = false;
}

FString UJusyncPlaybackWidget::MakeLiveOptionLabel()
{
    return FString(LiveOptionLabel);
}

void UJusyncPlaybackWidget::HandleSliderChanged(float Value)
{
    if (bApplyingFromCode || !Spawner)
    {
        return;
    }

    Spawner->SetPlaybackPosition(Value);
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
        Spawner->PlaybackPreviousSample();
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleNextClicked()
{
    if (Spawner)
    {
        Spawner->PlaybackNextSample();
        Refresh();
    }
}

void UJusyncPlaybackWidget::HandleLiveClicked()
{
    if (Spawner)
    {
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

    if (Spawner->IsPlayingCachedSample())
    {
        Spawner->FollowLatestSample();
    }
    else
    {
        // Park on the newest cached sample: below 0.999 the spawner enters
        // playback mode and stops applying incoming samples.
        Spawner->SetPlaybackPosition(FJusyncPlaybackHUDModel::LiveEdgeFreezePosition());
    }
    Refresh();
}

void UJusyncPlaybackWidget::HandlePathlineClicked()
{
    if (!Spawner)
    {
        return;
    }

    // Tick -> SyncPathlineManager pushes BuildPathlineSettings() every frame,
    // so flipping the property takes effect within one frame: off tears the
    // trail actors down, on starts rebuilding trails from the next sample.
    Spawner->bEnablePathlines = !Spawner->bEnablePathlines;
    Refresh();
}

void UJusyncPlaybackWidget::HandleTimestepSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || !Spawner || SelectionType == ESelectInfo::Direct)
    {
        return;
    }

    const int32 SampleIndex = FJusyncPlaybackHUDModel::ComboSelectionToSampleIndex(
        TimestepCombo->GetSelectedIndex());
    if (SampleIndex < 0)
    {
        Spawner->FollowLatestSample();
    }
    else
    {
        const int32 Count = Spawner->GetCachedSampleCount();
        Spawner->SetPlaybackPosition(
            FJusyncPlaybackHUDModel::SampleIndexToScrubPosition(SampleIndex, Count));
    }

    Refresh();
}

#undef LOCTEXT_NAMESPACE
