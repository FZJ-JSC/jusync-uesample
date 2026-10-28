#include "JusyncShortcutHintWidget.h"

#include "JusyncPlaybackHUDPlayerController.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"

#define LOCTEXT_NAMESPACE "JusyncShortcutHintWidget"

namespace
{

    namespace Theme
    {
        const FLinearColor PanelFill(0.030f, 0.035f, 0.045f, 0.30f);
        const FLinearColor PanelEdge(1.0f, 1.0f, 1.0f, 0.18f);
        const FLinearColor Divider(1.0f, 1.0f, 1.0f, 0.16f);

        const FLinearColor TextPrimary(0.94f, 0.95f, 0.97f, 1.0f);
        const FLinearColor TextMuted(0.60f, 0.64f, 0.70f, 1.0f);
        const FLinearColor TextShadow(0.0f, 0.0f, 0.0f, 0.65f);

        const FLinearColor ChipIdle(0.150f, 0.170f, 0.205f, 0.92f);
        const FLinearColor GlassRim(1.0f, 1.0f, 1.0f, 0.16f);

        constexpr float V1 = 24.0f;
        constexpr float R1 = 12.0f;
        constexpr float R3 = 6.0f;
        constexpr float H8 = 44.0f;
        constexpr float Gap = 8.0f;
        constexpr float HUDBlur = 10.0f;

        constexpr float CardWidth = 214.0f;
        constexpr float CardHeight = 96.0f;
        constexpr float KeyCapHeight = 22.0f;
        constexpr float RowHeight = 24.0f;

        constexpr float HeaderSize = 9.0f;
        constexpr float LabelSize = 11.0f;
        constexpr float KeySize = 10.0f;
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
}

void UJusyncShortcutHintWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    EnsureLayoutBuilt();
    RefreshKeys();
}

void UJusyncShortcutHintWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIntroStarted)
    {
        bIntroStarted = true;
        Reveal.Show(0.55f);
    }

    if (Reveal.Tick(InDeltaTime))
    {
        ApplyReveal();
    }
}

void UJusyncShortcutHintWidget::ApplyReveal()
{
    JusyncApplyPanelReveal(Reveal, WidgetTree ? WidgetTree->RootWidget : nullptr,
        FVector2D(0.0f, 8.0f), GlassBlurs);
}

void UJusyncShortcutHintWidget::EnsureLayoutBuilt()
{
    if (bLayoutBuilt || !WidgetTree)
    {
        return;
    }

    bLayoutBuilt = true;
    BuildLayout();
}

void UJusyncShortcutHintWidget::RefreshKeys()
{
    if (!PlaybackKeyText || !PathlineKeyText)
    {
        return;
    }

    auto KeyName = [](const FKey& Key)
    {
        return Key.IsValid()
            ? Key.GetDisplayName().ToString().ToUpper()
            : FString(TEXT("?"));
    };

    PlaybackKeyText->SetText(FText::FromString(KeyName(EKeys::T)));
    PathlineKeyText->SetText(FText::FromString(KeyName(EKeys::P)));

    if (const AJusyncPlaybackHUDPlayerController* Controller =
        Cast<AJusyncPlaybackHUDPlayerController>(GetOwningPlayer()))
    {
        PlaybackKeyText->SetText(FText::FromString(KeyName(Controller->ToggleKey)));
        PathlineKeyText->SetText(FText::FromString(KeyName(Controller->PathlineToggleKey)));
    }
}

UTextBlock* UJusyncShortcutHintWidget::MakeLabel(const FName Name, const FText& Label)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(Label);
    return Text;
}

UWidget* UJusyncShortcutHintWidget::MakeRow(const FName BaseName, UTextBlock*& OutKey,
    const FText& Caption)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), BaseName);
    Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UBorder* Cap = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
    Cap->SetBrush(FSlateRoundedBoxBrush(Theme::ChipIdle, Theme::R3, Theme::GlassRim, 1.0f));
    Cap->SetPadding(FMargin(7.0f, 0.0f, 7.0f, 0.0f));
    Cap->SetHorizontalAlignment(HAlign_Center);
    Cap->SetVerticalAlignment(VAlign_Center);

    UTextBlock* KeyText = MakeLabel(NAME_None, FText::FromString(TEXT("?")));
    StyleLabel(KeyText, Theme::KeySize, Theme::TextPrimary);
    KeyText->SetJustification(ETextJustify::Center);
    Cap->AddChild(KeyText);
    OutKey = KeyText;

    USizeBox* CapBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
    CapBox->SetMinDesiredWidth(26.0f);
    CapBox->SetHeightOverride(Theme::KeyCapHeight);
    CapBox->AddChild(Cap);

    UHorizontalBoxSlot* CapSlot = Row->AddChildToHorizontalBox(CapBox);
    CapSlot->SetVerticalAlignment(VAlign_Center);

    USpacer* GapSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), NAME_None);
    GapSpacer->SetSize(FVector2D(10.0f, 1.0f));
    UHorizontalBoxSlot* GapSlot = Row->AddChildToHorizontalBox(GapSpacer);
    GapSlot->SetVerticalAlignment(VAlign_Center);

    UTextBlock* Label = MakeLabel(NAME_None, Caption);
    StyleLabel(Label, Theme::LabelSize, Theme::TextPrimary);
    Label->SetJustification(ETextJustify::Left);
    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
    LabelSlot->SetVerticalAlignment(VAlign_Center);

    return Row;
}

void UJusyncShortcutHintWidget::BuildLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;

    const FVector2D CardPos(Theme::V1, Theme::V1 + Theme::H8 + Theme::Gap);

    UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(
        UBackgroundBlur::StaticClass(), NAME_None);
    Blur->SetPadding(FMargin(0.0f));
    Blur->SetBlurStrength(Theme::HUDBlur);
    Blur->SetCornerRadius(FVector4(Theme::R1, Theme::R1, Theme::R1, Theme::R1));
    Blur->SetApplyAlphaToBlur(false);
    Blur->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    GlassBlurs.Add({ Blur, Theme::HUDBlur });
    UCanvasPanelSlot* BlurSlot = Root->AddChildToCanvas(Blur);
    BlurSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    BlurSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    BlurSlot->SetPosition(CardPos);
    BlurSlot->SetSize(FVector2D(Theme::CardWidth, Theme::CardHeight));
    BlurSlot->SetZOrder(0);

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card"));
    Card->SetBrush(FSlateRoundedBoxBrush(Theme::PanelFill, Theme::R1, Theme::PanelEdge, 1.0f));
    Card->SetPadding(FMargin(12.0f, 10.0f, 12.0f, 10.0f));
    Card->SetHorizontalAlignment(HAlign_Fill);
    Card->SetVerticalAlignment(VAlign_Top);
    Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Card);
    CardSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    CardSlot->SetAlignment(FVector2D(0.0f, 0.0f));
    CardSlot->SetPosition(CardPos);
    CardSlot->SetSize(FVector2D(Theme::CardWidth, Theme::CardHeight));
    CardSlot->SetZOrder(1);

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("Column"));
    Card->AddChild(Column);

    UTextBlock* Header = MakeLabel(TEXT("Header"), LOCTEXT("Header", "SHORTCUTS"));
    StyleLabel(Header, Theme::HeaderSize, Theme::TextMuted);
    Header->SetJustification(ETextJustify::Left);
    UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(Header);
    HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
    HeaderSlot->SetHorizontalAlignment(HAlign_Left);

    UTextBlock* PlaybackKey = nullptr;
    UVerticalBoxSlot* PlaybackSlot = Column->AddChildToVerticalBox(
        MakeRow(TEXT("PlaybackRow"), PlaybackKey, LOCTEXT("Playback", "Playback")));
    PlaybackSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    PlaybackSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    PlaybackSlot->SetVerticalAlignment(VAlign_Center);
    PlaybackKeyText = PlaybackKey;

    UTextBlock* PathlineKey = nullptr;
    UVerticalBoxSlot* PathlineSlot = Column->AddChildToVerticalBox(
        MakeRow(TEXT("PathlineRow"), PathlineKey, LOCTEXT("Pathlines", "Pathlines")));
    PathlineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    PathlineSlot->SetVerticalAlignment(VAlign_Center);
    PathlineKeyText = PathlineKey;

    ApplyReveal();
}

#undef LOCTEXT_NAMESPACE
