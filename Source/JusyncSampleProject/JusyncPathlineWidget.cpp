#include "JusyncPathlineWidget.h"

#include "JusyncHUDIcons.h"
#include "JusyncPathlineHUDModel.h"
#include "JusyncPlaybackHUDPlayerController.h"
#include "JUSYNCFileSpawnerActor.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "HAL/IConsoleManager.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "JusyncPathlineWidget"

namespace
{

    constexpr float PointGlowMax = 4000.0f;

    float PointGlowFromSlider(float Value)
    {
        const float V = FMath::Clamp(Value, 0.0f, 1.0f);
        return PointGlowMax * V * V;
    }

    float PointGlowToSlider(float Glow)
    {
        return FMath::Clamp(FMath::Sqrt(FMath::Max(Glow, 0.0f) / PointGlowMax), 0.0f, 1.0f);
    }

    constexpr float PathlineGlowMax = 2000.0f;

    float PathlineGlowFromSlider(float Value)
    {
        const float V = FMath::Clamp(Value, 0.0f, 1.0f);
        return PathlineGlowMax * V * V;
    }

    float PathlineGlowToSlider(float Glow)
    {
        return FMath::Clamp(FMath::Sqrt(FMath::Max(Glow, 0.0f) / PathlineGlowMax), 0.0f, 1.0f);
    }

    namespace Theme
    {
        const FLinearColor PanelEdge(1.0f, 1.0f, 1.0f, 0.11f);
        const FLinearColor PanelHeaderFill(0.070f, 0.080f, 0.098f, 0.95f);
        const FLinearColor GlassTint(0.035f, 0.042f, 0.055f, 0.34f);
        const FLinearColor GlassRim(1.0f, 1.0f, 1.0f, 0.20f);
        const FLinearColor CardFill(0.028f, 0.034f, 0.045f, 0.36f);
        const FLinearColor CardHeaderFill(0.045f, 0.052f, 0.066f, 0.46f);
        const FLinearColor CardEdge(1.0f, 1.0f, 1.0f, 0.18f);
        const FLinearColor Divider(1.0f, 1.0f, 1.0f, 0.11f);

        const FLinearColor TextPrimary(0.94f, 0.95f, 0.97f, 1.0f);
        const FLinearColor TextMuted(0.70f, 0.74f, 0.80f, 1.0f);
        const FLinearColor TextShadow(0.0f, 0.0f, 0.0f, 0.65f);

        const FLinearColor Live(0.35f, 0.85f, 0.45f, 1.0f);
        const FLinearColor Paused(1.0f, 0.72f, 0.24f, 1.0f);
        const FLinearColor Accent(0.28f, 0.62f, 1.00f, 1.0f);

        const FLinearColor ChipIdle(0.150f, 0.170f, 0.205f, 0.92f);
        const FLinearColor ChipHover(0.220f, 0.250f, 0.295f, 1.00f);
        const FLinearColor ChipPressed(0.200f, 0.450f, 0.850f, 1.00f);
        const FLinearColor ChipDisabled(0.100f, 0.110f, 0.130f, 0.45f);
        const FLinearColor WellFill(0.025f, 0.030f, 0.040f, 0.92f);
        const FLinearColor WellEdge(1.0f, 1.0f, 1.0f, 0.07f);
        const FLinearColor SliderTrack(1.0f, 1.0f, 1.0f, 0.30f);
        const FLinearColor SliderThumb(0.88f, 0.92f, 1.00f, 1.0f);

        constexpr float V1 = 24.0f;
        constexpr float W = 360.0f;
        constexpr float H = 716.0f;
        constexpr float H1 = 60.0f;
        constexpr float H2 = 52.0f;
        constexpr float H3 = 28.0f;
        constexpr float H4 = 40.0f;
        constexpr float H6 = 40.0f;
        constexpr float R1 = 12.0f;
        constexpr float R2 = 7.0f;
        constexpr float R3 = 6.0f;
        constexpr float GlassBlur = 14.0f;
        constexpr float S1 = 16.0f;
        constexpr float S2 = 12.0f;
        constexpr float S3 = 8.0f;
        constexpr float S4 = 20.0f;
        constexpr float S7 = 12.0f;
        constexpr float LabelWidth = 92.0f;
        constexpr float ValueWidth = 48.0f;
        constexpr float CardContentHeight = 255.0f;
        constexpr float PointCardContentHeight = 255.0f;
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
        const FSlateRoundedBoxBrush TrackDisabled(Theme::SliderTrack * 0.9f, 3.0f,
            FLinearColor::Transparent, 0.0f, FVector2D(0.0f, 6.0f));
        const FSlateRoundedBoxBrush Thumb(Theme::SliderThumb, 7.0f,
            FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 1.0f, FVector2D(14.0f, 14.0f));
        const FSlateRoundedBoxBrush ThumbHover(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f), 7.0f,
            FLinearColor(0.0f, 0.0f, 0.0f, 0.25f), 1.0f, FVector2D(14.0f, 14.0f));
        const FSlateRoundedBoxBrush ThumbDisabled(FLinearColor(0.55f, 0.58f, 0.62f, 0.70f), 7.0f,
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

    FComboBoxStyle MakeComboStyle()
    {
        FComboBoxStyle Style = FComboBoxStyle::GetDefault();

        FButtonStyle Button;
        Button.Normal = FSlateRoundedBoxBrush(Theme::WellFill, 6.0f, Theme::WellEdge, 1.0f);
        Button.Hovered = FSlateRoundedBoxBrush(Theme::ChipHover, 6.0f,
            FLinearColor(1.0f, 1.0f, 1.0f, 0.16f), 1.0f);
        Button.Pressed = FSlateRoundedBoxBrush(Theme::ChipPressed, 6.0f,
            FLinearColor(1.0f, 1.0f, 1.0f, 0.20f), 1.0f);
        Button.Disabled = FSlateRoundedBoxBrush(Theme::ChipDisabled, 6.0f,
            FLinearColor::Transparent, 0.0f);
        Button.NormalPadding = FMargin(0.0f);
        Button.PressedPadding = FMargin(0.0f);

        Style.ComboButtonStyle.ButtonStyle = Button;
        Style.ComboButtonStyle.ContentPadding = FMargin(10.0f, 0.0f, 6.0f, 0.0f);
        Style.ComboButtonStyle.DownArrowPadding = FMargin(0.0f, 0.0f, 8.0f, 0.0f);
        if (UTexture2D* ArrowTexture = JusyncHUDIcons::GetTexture(
            JusyncHUDIcons::EIcon::ChevronDown))
        {
            Style.ComboButtonStyle.DownArrowImage = FSlateImageBrush(
                ArrowTexture, FVector2D(12.0f, 12.0f), Theme::TextMuted);
        }
        Style.ComboButtonStyle.MenuBorderBrush = FSlateRoundedBoxBrush(
            Theme::PanelHeaderFill, 6.0f, Theme::PanelEdge, 1.0f);
        Style.ComboButtonStyle.MenuBorderPadding = FMargin(1.0f);
        Style.ContentPadding = FMargin(10.0f, 0.0f, 6.0f, 0.0f);
        Style.MenuRowPadding = FMargin(8.0f, 4.0f, 8.0f, 4.0f);
        return Style;
    }

    FTableRowStyle MakeComboItemStyle()
    {
        FTableRowStyle Style = FTableRowStyle::GetDefault();
        Style.TextColor = FSlateColor(Theme::TextPrimary);
        Style.SelectedTextColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
        Style.ActiveBrush = FSlateRoundedBoxBrush(Theme::Accent, 4.0f);
        Style.ActiveHoveredBrush = FSlateRoundedBoxBrush(Theme::Accent * 1.15f, 4.0f);
        Style.InactiveBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
        Style.InactiveHoveredBrush = FSlateRoundedBoxBrush(Theme::ChipHover, 4.0f);
        Style.EvenRowBackgroundBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
        Style.OddRowBackgroundBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 0.0f);
        Style.EvenRowBackgroundHoveredBrush = FSlateRoundedBoxBrush(Theme::ChipHover, 0.0f);
        Style.OddRowBackgroundHoveredBrush = FSlateRoundedBoxBrush(Theme::ChipHover, 0.0f);
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

    UBackgroundBlur* MakeGlassBlur(UWidgetTree* Tree, float Radius, float Strength)
    {
        UBackgroundBlur* Blur = Tree->ConstructWidget<UBackgroundBlur>(
            UBackgroundBlur::StaticClass(), NAME_None);
        Blur->SetPadding(FMargin(0.0f));
        Blur->SetBlurStrength(Strength);
        Blur->SetCornerRadius(FVector4(Radius, Radius, Radius, Radius));
        Blur->SetApplyAlphaToBlur(false);
        Blur->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        return Blur;
    }

    UButton* MakeIconButton(UWidgetTree* Tree, const FName Name,
        JusyncHUDIcons::EIcon Icon, const FText& Tooltip, float IconSize)
    {
        UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        Button->SetToolTipText(Tooltip);
        Button->SetStyle(MakeButtonStyle(Theme::ChipIdle, Theme::ChipHover,
            Theme::ChipPressed, Theme::ChipDisabled, 6.0f));
        Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

        UImage* Image = JusyncHUDIcons::MakeImage(Tree, Icon, IconSize, Theme::TextPrimary);
        Button->AddChild(Image);
        if (UButtonSlot* ImageSlot = Cast<UButtonSlot>(Image->Slot))
        {
            ImageSlot->SetPadding(FMargin(0.0f));
            ImageSlot->SetHorizontalAlignment(HAlign_Center);
            ImageSlot->SetVerticalAlignment(VAlign_Center);
        }
        return Button;
    }

    void DisableTabNavigation(UWidget* Widget)
    {
        if (!Widget)
        {
            return;
        }
        Widget->SetNavigationRuleBase(EUINavigation::Next, EUINavigationRule::Escape);
        Widget->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Escape);
    }
}

void UJusyncPathlineWidget::BindSpawner(AJUSYNCFileSpawnerActor* InSpawner)
{
    Spawner = InSpawner;
    EnsureLayoutBuilt();

    if (Spawner && Spawner->bCloudFixedSize)
    {
        Spawner->PointScaling = EJUSYNCPointScaling::FixedScreenSize;
    }
    SyncPointCloudFixedSizeMode();

    Refresh();
}

void UJusyncPathlineWidget::EnsureLayoutBuilt()
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
             static_cast<UWidget*>(EnableSwitch.Get()),
             static_cast<UWidget*>(PointsSwitch.Get()),
             static_cast<UWidget*>(HistorySlider.Get()),
             static_cast<UWidget*>(WidthSlider.Get()),
             static_cast<UWidget*>(PathlineGlowSlider.Get()),
             static_cast<UWidget*>(PointSizeSlider.Get()),
             static_cast<UWidget*>(PointGlowSlider.Get()),
             static_cast<UWidget*>(ColorCombo.Get()),
             static_cast<UWidget*>(HueCombo.Get()),
             static_cast<UWidget*>(ShapeSquareButton.Get()),
             static_cast<UWidget*>(ShapeCircleButton.Get()),
             static_cast<UWidget*>(ScalingCombo.Get()),
             static_cast<UWidget*>(OrientationCombo.Get()),
             static_cast<UWidget*>(PointHueCombo.Get()),
             static_cast<UWidget*>(NormalsSwitch.Get()),
             static_cast<UWidget*>(MaxMinusButton.Get()),
             static_cast<UWidget*>(MaxPlusButton.Get()),
             static_cast<UWidget*>(ExpiryMinusButton.Get()),
             static_cast<UWidget*>(ExpiryPlusButton.Get()),
             static_cast<UWidget*>(QualityMinusButton.Get()),
             static_cast<UWidget*>(QualityPlusButton.Get())})
    {
        DisableTabNavigation(Control);
    }

    if (EnableSwitch)
    {
        EnableSwitch->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleEnableClicked);
    }
    if (PointsSwitch)
    {
        PointsSwitch->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointsEnableClicked);
    }
    if (HistorySlider)
    {
        HistorySlider->OnValueChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleHistoryChanged);
    }
    if (WidthSlider)
    {
        WidthSlider->OnValueChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleWidthChanged);
    }
    if (PathlineGlowSlider)
    {
        PathlineGlowSlider->OnValueChanged.AddDynamic(this,
            &UJusyncPathlineWidget::HandlePathlineGlowChanged);
    }
    if (PointSizeSlider)
    {
        PointSizeSlider->OnValueChanged.AddDynamic(this, &UJusyncPathlineWidget::HandlePointSizeChanged);
    }
    if (PointGlowSlider)
    {
        PointGlowSlider->OnValueChanged.AddDynamic(this, &UJusyncPathlineWidget::HandlePointGlowChanged);
    }
    if (ColorCombo)
    {
        ColorCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleColorSelected);
    }
    if (HueCombo)
    {
        HueCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleHueSelected);
    }
    if (ShapeSquareButton)
    {
        ShapeSquareButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleShapeSquareClicked);
    }
    if (ShapeCircleButton)
    {
        ShapeCircleButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleShapeCircleClicked);
    }
    if (ScalingCombo)
    {
        ScalingCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleScalingSelected);
    }
    if (OrientationCombo)
    {
        OrientationCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPathlineWidget::HandleOrientationSelected);
    }
    if (PointHueCombo)
    {
        PointHueCombo->OnSelectionChanged.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueSelected);
    }
    if (NormalsSwitch)
    {
        NormalsSwitch->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleNormalsClicked);
    }
    if (MaxMinusButton)
    {
        MaxMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleMaxMinus);
    }
    if (MaxPlusButton)
    {
        MaxPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleMaxPlus);
    }
    if (ExpiryMinusButton)
    {
        ExpiryMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleExpiryMinus);
    }
    if (ExpiryPlusButton)
    {
        ExpiryPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleExpiryPlus);
    }
    if (QualityMinusButton)
    {
        QualityMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleQualityMinus);
    }
    if (QualityPlusButton)
    {
        QualityPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleQualityPlus);
    }

    Refresh();
}

void UJusyncPathlineWidget::Open()
{
    EnsureLayoutBuilt();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Reveal.Show();
    ApplyReveal();
}

void UJusyncPathlineWidget::Close()
{
    if (Reveal.IsClosed())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    Reveal.Hide();
    ApplyReveal();
}

void UJusyncPathlineWidget::ApplyReveal()
{
    JusyncApplyPanelReveal(Reveal, WidgetTree ? WidgetTree->RootWidget : nullptr,
        FVector2D(16.0f, 0.0f), GlassBlurs);
}

void UJusyncPathlineWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureLayoutBuilt();
}

void UJusyncPathlineWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (Reveal.Tick(InDeltaTime))
    {
        ApplyReveal();
        if (Reveal.IsClosed())
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    RefreshAccumulator += InDeltaTime;
    if (RefreshAccumulator < RefreshIntervalSeconds)
    {
        return;
    }
    RefreshAccumulator = 0.0f;
    Refresh();
}

FReply UJusyncPathlineWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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

void UJusyncPathlineWidget::BuildLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    Panel = MakeRoundedBorder(TEXT("Panel"), FLinearColor::Transparent,
        FLinearColor::Transparent, 0.0f, 0.0f);
    Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    PanelSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    PanelSlot->SetPosition(FVector2D(-Theme::V1, Theme::V1));
    PanelSlot->SetSize(FVector2D(Theme::W, Theme::H));

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PanelStack"));
    Panel->AddChild(Stack);

    auto MakeGlassSurface = [this](UWidget* Content, const FLinearColor& Tint,
        const FLinearColor& Rim, float Radius, const FMargin& ContentPadding,
        float Strength) -> UOverlay*
    {
        UOverlay* Surface = WidgetTree->ConstructWidget<UOverlay>(
            UOverlay::StaticClass(), NAME_None);
        Surface->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        UBackgroundBlur* Blur = MakeGlassBlur(WidgetTree, Radius, Strength);
        GlassBlurs.Add({ Blur, Strength });
        if (UOverlaySlot* BlurSlot = Surface->AddChildToOverlay(Blur))
        {
            BlurSlot->SetHorizontalAlignment(HAlign_Fill);
            BlurSlot->SetVerticalAlignment(VAlign_Fill);
            BlurSlot->SetPadding(FMargin(0.0f));
        }

        UBorder* TintBorder = MakeRoundedBorder(NAME_None, Tint, Rim, Radius, 1.0f, ContentPadding);
        TintBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        TintBorder->AddChild(Content);
        if (UOverlaySlot* TintSlot = Surface->AddChildToOverlay(TintBorder))
        {
            TintSlot->SetHorizontalAlignment(HAlign_Fill);
            TintSlot->SetVerticalAlignment(VAlign_Fill);
            TintSlot->SetPadding(FMargin(0.0f));
        }
        return Surface;
    };

    UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("LayersHeader"));
    UOverlay* Header = MakeGlassSurface(HeaderRow, Theme::GlassTint, Theme::GlassRim,
        Theme::R1, FMargin(Theme::S2, 0.0f, Theme::S2, 0.0f), Theme::GlassBlur);
    USizeBox* HeaderBox = MakeSizeBox(WidgetTree, Header, -1.0f, Theme::H1);
    Stack->AddChildToVerticalBox(HeaderBox);

    UTextBlock* HeaderTitle = MakeLabel(TEXT("LayersTitle"), FText::FromString(TEXT("LAYERS")));
    StyleLabel(HeaderTitle, 13.0f, Theme::TextPrimary);
    HeaderRow->AddChildToHorizontalBox(HeaderTitle)->SetVerticalAlignment(VAlign_Center);

    USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    UHorizontalBoxSlot* HeaderSpacerSlot = HeaderRow->AddChildToHorizontalBox(HeaderSpacer);
    HeaderSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    ActiveText = MakeLabel(TEXT("ActiveText"), FText::FromString(TEXT("2 ACTIVE")));
    StyleLabel(ActiveText, 10.0f, Theme::TextMuted);
    ActiveText->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* ActiveSlot = HeaderRow->AddChildToHorizontalBox(ActiveText);
    ActiveSlot->SetVerticalAlignment(VAlign_Center);

    UImage* Chevron = JusyncHUDIcons::MakeImage(WidgetTree,
        JusyncHUDIcons::EIcon::ChevronDown, 12.0f, Theme::TextMuted);
    UHorizontalBoxSlot* ChevronSlot = HeaderRow->AddChildToHorizontalBox(Chevron);
    ChevronSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
    ChevronSlot->SetVerticalAlignment(VAlign_Center);

    UVerticalBox* PathlineCardStack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PathlineCardStack"));
    UOverlay* PathlineCard = MakeGlassSurface(PathlineCardStack, Theme::CardFill,
        Theme::CardEdge, Theme::R2, FMargin(0.0f), Theme::GlassBlur);

    UBorder* PathlineHeader = MakeRoundedBorder(NAME_None, Theme::CardHeaderFill,
        FLinearColor::Transparent, Theme::R2, 0.0f, FMargin(Theme::S2, 0.0f, Theme::S2, 0.0f));
    USizeBox* PathlineHeaderBox = MakeSizeBox(WidgetTree, PathlineHeader, -1.0f, Theme::H2);
    PathlineCardStack->AddChildToVerticalBox(PathlineHeaderBox);

    UHorizontalBox* PathlineHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PathlineHeaderRow"));
    PathlineHeader->AddChild(PathlineHeaderRow);

    UTextBlock* PathlineTitle = MakeLabel(TEXT("PathlineTitle"),
        FText::FromString(TEXT("PATHLINES")));
    StyleLabel(PathlineTitle, 11.0f, Theme::TextPrimary);
    PathlineHeaderRow->AddChildToHorizontalBox(PathlineTitle)->SetVerticalAlignment(VAlign_Center);

    USpacer* PathlineSpacer = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    UHorizontalBoxSlot* PathlineSpacerSlot = PathlineHeaderRow->AddChildToHorizontalBox(PathlineSpacer);
    PathlineSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    PathlineStatusText = MakeLabel(TEXT("PathlineStatus"), FText::FromString(TEXT("0 TRACKS")));
    StyleLabel(PathlineStatusText, 10.0f, Theme::TextMuted);
    PathlineStatusText->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* PathlineStatusSlot = PathlineHeaderRow->AddChildToHorizontalBox(PathlineStatusText);
    PathlineStatusSlot->SetPadding(FMargin(8.0f, 0.0f, 10.0f, 0.0f));
    PathlineStatusSlot->SetVerticalAlignment(VAlign_Center);

    EnableSwitch = MakeSwitch(TEXT("EnableSwitch"),
        LOCTEXT("EnableTip", "Show or hide the Niagara pathline ribbons"), EnableThumb);
    USizeBox* EnableSwitchBox = MakeSizeBox(WidgetTree, EnableSwitch, 36.0f, 20.0f);
    UHorizontalBoxSlot* EnableSwitchSlot = PathlineHeaderRow->AddChildToHorizontalBox(EnableSwitchBox);
    EnableSwitchSlot->SetVerticalAlignment(VAlign_Center);

    USizeBox* PathlineRuleBox = MakeSizeBox(WidgetTree,
        MakeRoundedBorder(NAME_None, Theme::Divider, FLinearColor::Transparent, 0.0f, 0.0f),
        -1.0f, 1.0f);
    PathlineCardStack->AddChildToVerticalBox(PathlineRuleBox);

    UBorder* PathlineContent = MakeRoundedBorder(NAME_None, FLinearColor::Transparent,
        FLinearColor::Transparent, 0.0f, 0.0f, FMargin(Theme::S2, 6.0f, Theme::S2, 6.0f));
    USizeBox* PathlineContentBox = MakeSizeBox(WidgetTree, PathlineContent, -1.0f,
        Theme::CardContentHeight);
    PathlineCardStack->AddChildToVerticalBox(PathlineContentBox);

    UVerticalBox* PathlineRows = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PathlineRows"));
    PathlineContent->AddChild(PathlineRows);

    auto AddGap = [this](UVerticalBox* Rows)
    {
        USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), NAME_None);
        Gap->SetSize(FVector2D(0.0f, Theme::S3));
        Rows->AddChildToVerticalBox(Gap);
    };

    AddSliderRow(PathlineRows, FText::FromString(TEXT("TRAIL LIFE")), TEXT("HistorySlider"),
        2.0f, 512.0f, 1.0f, HistorySlider, HistoryValueText);
    AddGap(PathlineRows);
    AddSliderRow(PathlineRows, FText::FromString(TEXT("WIDTH")), TEXT("WidthSlider"),
        0.1f, 5.0f, 0.05f, WidthSlider, WidthValueText);
    AddGap(PathlineRows);

    AddSliderRow(PathlineRows, FText::FromString(TEXT("GLOW")), TEXT("PathlineGlowSlider"),
        0.0f, 1.0f, 0.005f, PathlineGlowSlider, PathlineGlowValueText);
    AddGap(PathlineRows);

    UHorizontalBox* PathlineRow3 = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PathlineRow3"));
    PathlineRows->AddChildToVerticalBox(PathlineRow3);

    UVerticalBox* ColorColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ColorColumn"));
    USizeBox* ColorColumnBox = MakeSizeBox(WidgetTree, ColorColumn, 144.0f, Theme::H4);
    UHorizontalBoxSlot* ColorColumnSlot = PathlineRow3->AddChildToHorizontalBox(ColorColumnBox);
    ColorColumnSlot->SetVerticalAlignment(VAlign_Top);

    AddColumnLabel(ColorColumn, FText::FromString(TEXT("COLOR")));
    ColorCombo = MakeCombo(TEXT("ColorCombo"), { TEXT("SCALAR"), TEXT("FLAT") }, 0);
    ColorColumn->AddChildToVerticalBox(MakeSizeBox(WidgetTree, ColorCombo, 144.0f, Theme::H3));

    USpacer* PathlineGap3 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    PathlineGap3->SetSize(FVector2D(Theme::S4, 0.0f));
    PathlineRow3->AddChildToHorizontalBox(PathlineGap3);

    UVerticalBox* HueColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("HueColumn"));
    USizeBox* HueColumnBox = MakeSizeBox(WidgetTree, HueColumn, 172.0f, Theme::H4);
    PathlineRow3->AddChildToHorizontalBox(HueColumnBox)->SetVerticalAlignment(VAlign_Top);

    AddColumnLabel(HueColumn, FText::FromString(TEXT("HUE")));
    HueCombo = MakeCombo(TEXT("HueCombo"),
        { TEXT("OFF"), TEXT("RED"), TEXT("YELLOW"), TEXT("GREEN"), TEXT("CYAN"),
          TEXT("BLUE"), TEXT("MAGENTA") }, 0);
    HueColumn->AddChildToVerticalBox(MakeSizeBox(WidgetTree, HueCombo, 172.0f, Theme::H3));

    AddGap(PathlineRows);

    UHorizontalBox* PathlineRow4 = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PathlineRow4"));
    PathlineRows->AddChildToVerticalBox(PathlineRow4);

    UVerticalBox* MaxColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("MaxColumn"));
    USizeBox* MaxColumnBox = MakeSizeBox(WidgetTree, MaxColumn, 144.0f, Theme::H4);
    UHorizontalBoxSlot* MaxColumnSlot = PathlineRow4->AddChildToHorizontalBox(MaxColumnBox);
    MaxColumnSlot->SetVerticalAlignment(VAlign_Top);

    AddColumnLabel(MaxColumn, FText::FromString(TEXT("MAX")));
    MaxColumn->AddChildToVerticalBox(MakeStepper(TEXT("Max"), 144.0f,
        MaxMinusButton, MaxPlusButton, MaxValueText));

    USpacer* PathlineGap4 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    PathlineGap4->SetSize(FVector2D(Theme::S4, 0.0f));
    PathlineRow4->AddChildToHorizontalBox(PathlineGap4);

    UVerticalBox* ExpiryColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ExpiryColumn"));
    USizeBox* ExpiryColumnBox = MakeSizeBox(WidgetTree, ExpiryColumn, 172.0f, Theme::H4);
    PathlineRow4->AddChildToHorizontalBox(ExpiryColumnBox)->SetVerticalAlignment(VAlign_Top);

    AddColumnLabel(ExpiryColumn, FText::FromString(TEXT("EXPIRY")));
    ExpiryColumn->AddChildToVerticalBox(MakeStepper(TEXT("Expiry"), 172.0f,
        ExpiryMinusButton, ExpiryPlusButton, ExpiryValueText));

    UVerticalBoxSlot* PathlineCardSlot = Stack->AddChildToVerticalBox(PathlineCard);
    PathlineCardSlot->SetPadding(FMargin(0.0f, Theme::S7, 0.0f, 0.0f));

    UVerticalBox* PointCardStack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PointCardStack"));
    UOverlay* PointCard = MakeGlassSurface(PointCardStack, Theme::CardFill,
        Theme::CardEdge, Theme::R2, FMargin(0.0f), Theme::GlassBlur);

    UBorder* PointHeader = MakeRoundedBorder(NAME_None, Theme::CardHeaderFill,
        FLinearColor::Transparent, Theme::R2, 0.0f, FMargin(Theme::S2, 0.0f, Theme::S2, 0.0f));
    USizeBox* PointHeaderBox = MakeSizeBox(WidgetTree, PointHeader, -1.0f, Theme::H2);
    PointCardStack->AddChildToVerticalBox(PointHeaderBox);

    UHorizontalBox* PointHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointHeaderRow"));
    PointHeader->AddChild(PointHeaderRow);

    UTextBlock* PointTitle = MakeLabel(TEXT("PointTitle"),
        FText::FromString(TEXT("POINT CLOUD")));
    StyleLabel(PointTitle, 11.0f, Theme::TextPrimary);
    PointHeaderRow->AddChildToHorizontalBox(PointTitle)->SetVerticalAlignment(VAlign_Center);

    USpacer* PointSpacer = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    UHorizontalBoxSlot* PointSpacerSlot = PointHeaderRow->AddChildToHorizontalBox(PointSpacer);
    PointSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    PointStatusText = MakeLabel(TEXT("PointStatus"), FText::FromString(TEXT("0 ACTORS")));
    StyleLabel(PointStatusText, 10.0f, Theme::TextMuted);
    PointStatusText->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* PointStatusSlot = PointHeaderRow->AddChildToHorizontalBox(PointStatusText);
    PointStatusSlot->SetPadding(FMargin(8.0f, 0.0f, 10.0f, 0.0f));
    PointStatusSlot->SetVerticalAlignment(VAlign_Center);

    PointsSwitch = MakeSwitch(TEXT("PointsSwitch"),
        LOCTEXT("PointsTip", "Show or hide the streamed point clouds"), PointsThumb);
    USizeBox* PointsSwitchBox = MakeSizeBox(WidgetTree, PointsSwitch, 36.0f, 20.0f);
    UHorizontalBoxSlot* PointsSwitchSlot = PointHeaderRow->AddChildToHorizontalBox(PointsSwitchBox);
    PointsSwitchSlot->SetVerticalAlignment(VAlign_Center);

    USizeBox* PointRuleBox = MakeSizeBox(WidgetTree,
        MakeRoundedBorder(NAME_None, Theme::Divider, FLinearColor::Transparent, 0.0f, 0.0f),
        -1.0f, 1.0f);
    PointCardStack->AddChildToVerticalBox(PointRuleBox);

    UBorder* PointContent = MakeRoundedBorder(NAME_None, FLinearColor::Transparent,
        FLinearColor::Transparent, 0.0f, 0.0f, FMargin(Theme::S2, 6.0f, Theme::S2, 6.0f));
    USizeBox* PointContentBox = MakeSizeBox(WidgetTree, PointContent, -1.0f,
        Theme::PointCardContentHeight);
    PointCardStack->AddChildToVerticalBox(PointContentBox);

    UVerticalBox* PointRows = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PointRows"));
    PointContent->AddChild(PointRows);

    AddSliderRow(PointRows, FText::FromString(TEXT("POINT SIZE")), TEXT("PointSizeSlider"),
        0.001f, 1.0f, 0.001f, PointSizeSlider, PointSizeValueText);
    AddGap(PointRows);

    AddSliderRow(PointRows, FText::FromString(TEXT("GLOW")), TEXT("PointGlowSlider"),
        0.0f, 1.0f, 0.005f, PointGlowSlider, PointGlowValueText);
    AddGap(PointRows);

    UHorizontalBox* PointRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointRow2"));
    PointRows->AddChildToVerticalBox(PointRow2);

    UVerticalBox* ShapeColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ShapeColumn"));
    USizeBox* ShapeColumnBox = MakeSizeBox(WidgetTree, ShapeColumn, 144.0f, Theme::H4);
    UHorizontalBoxSlot* ShapeColumnSlot = PointRow2->AddChildToHorizontalBox(ShapeColumnBox);
    ShapeColumnSlot->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(ShapeColumn, FText::FromString(TEXT("SHAPE")));

    UBorder* ShapeWell = MakeRoundedBorder(NAME_None, Theme::WellFill, Theme::WellEdge,
        4.0f, 1.0f, FMargin(2.0f));
    UHorizontalBox* ShapeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("ShapeRow"));
    ShapeWell->AddChild(ShapeRow);

    ShapeSquareButton = MakeIconButton(WidgetTree, TEXT("ShapeSquare"),
        JusyncHUDIcons::EIcon::Square,
        LOCTEXT("ShapeSquareTip", "Use square points"), 13.0f);
    ShapeCircleButton = MakeIconButton(WidgetTree, TEXT("ShapeCircle"),
        JusyncHUDIcons::EIcon::Circle,
        LOCTEXT("ShapeCircleTip", "Use circular points"), 13.0f);
    ShapeSquareButton->SetStyle(MakeButtonStyle(FLinearColor::White, FLinearColor::White,
        FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), Theme::ChipDisabled, 4.0f));
    ShapeCircleButton->SetStyle(MakeButtonStyle(FLinearColor::White, FLinearColor::White,
        FLinearColor(0.85f, 0.85f, 0.85f, 1.0f), Theme::ChipDisabled, 4.0f));
    ShapeRow->AddChildToHorizontalBox(MakeSizeBox(WidgetTree, ShapeSquareButton, 58.0f, 24.0f))
        ->SetVerticalAlignment(VAlign_Center);
    ShapeRow->AddChildToHorizontalBox(MakeSizeBox(WidgetTree, ShapeCircleButton, 58.0f, 24.0f))
        ->SetVerticalAlignment(VAlign_Center);

    USizeBox* ShapeWellBox = MakeSizeBox(WidgetTree, ShapeWell, 120.0f, Theme::H3);
    UVerticalBoxSlot* ShapeWellSlot = ShapeColumn->AddChildToVerticalBox(ShapeWellBox);
    ShapeWellSlot->SetHorizontalAlignment(HAlign_Left);

    USpacer* PointGap2 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    PointGap2->SetSize(FVector2D(Theme::S4, 0.0f));
    PointRow2->AddChildToHorizontalBox(PointGap2);

    UVerticalBox* ScalingColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ScalingColumn"));
    USizeBox* ScalingColumnBox = MakeSizeBox(WidgetTree, ScalingColumn, 172.0f, Theme::H4);
    PointRow2->AddChildToHorizontalBox(ScalingColumnBox)->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(ScalingColumn, FText::FromString(TEXT("SCALING")));
    ScalingCombo = MakeCombo(TEXT("ScalingCombo"),
        { TEXT("NODE"), TEXT("ADAPT"), TEXT("POINT"), TEXT("SCREEN") }, 1);
    ScalingColumn->AddChildToVerticalBox(MakeSizeBox(WidgetTree, ScalingCombo, 172.0f, Theme::H3));

    AddGap(PointRows);

    UHorizontalBox* PointRow3 = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointRow3"));
    PointRows->AddChildToVerticalBox(PointRow3);

    UVerticalBox* OrientationColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("OrientationColumn"));
    USizeBox* OrientationColumnBox = MakeSizeBox(WidgetTree, OrientationColumn, 144.0f, Theme::H4);
    UHorizontalBoxSlot* OrientationColumnSlot = PointRow3->AddChildToHorizontalBox(OrientationColumnBox);
    OrientationColumnSlot->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(OrientationColumn, FText::FromString(TEXT("ORIENT")));
    OrientationCombo = MakeCombo(TEXT("OrientationCombo"),
        { TEXT("CAMERA"), TEXT("NORMAL") }, 0);
    OrientationColumn->AddChildToVerticalBox(MakeSizeBox(WidgetTree, OrientationCombo, 144.0f, Theme::H3));

    USpacer* PointGap3 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    PointGap3->SetSize(FVector2D(Theme::S4, 0.0f));
    PointRow3->AddChildToHorizontalBox(PointGap3);

    UVerticalBox* PointHueColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("PointHueColumn"));
    USizeBox* PointHueColumnBox = MakeSizeBox(WidgetTree, PointHueColumn, 172.0f, Theme::H4);
    PointRow3->AddChildToHorizontalBox(PointHueColumnBox)->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(PointHueColumn, FText::FromString(TEXT("FILTER")));

    PointHueCombo = MakeCombo(TEXT("PointHueCombo"),
        { TEXT("OFF"), TEXT("RED"), TEXT("YELLOW"), TEXT("GREEN"), TEXT("CYAN"),
          TEXT("BLUE"), TEXT("MAGENTA") }, 0);
    PointHueColumn->AddChildToVerticalBox(MakeSizeBox(WidgetTree, PointHueCombo, 172.0f, Theme::H3));

    AddGap(PointRows);

    UHorizontalBox* PointRow4 = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointRow4"));
    PointRows->AddChildToVerticalBox(PointRow4);

    UVerticalBox* NormalsColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("NormalsColumn"));
    USizeBox* NormalsColumnBox = MakeSizeBox(WidgetTree, NormalsColumn, 144.0f, Theme::H4);
    UHorizontalBoxSlot* NormalsColumnSlot = PointRow4->AddChildToHorizontalBox(NormalsColumnBox);
    NormalsColumnSlot->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(NormalsColumn, FText::FromString(TEXT("NORMALS")));
    NormalsSwitch = MakeSwitch(TEXT("NormalsSwitch"),
        LOCTEXT("NormalsTip", "Calculate point-cloud normals"), NormalsThumb);
    USizeBox* NormalsSwitchBox = MakeSizeBox(WidgetTree, NormalsSwitch, 36.0f, 20.0f);
    UVerticalBoxSlot* NormalsSwitchSlot = NormalsColumn->AddChildToVerticalBox(NormalsSwitchBox);
    NormalsSwitchSlot->SetHorizontalAlignment(HAlign_Left);

    USpacer* PointGap4 = WidgetTree->ConstructWidget<USpacer>(
        USpacer::StaticClass(), NAME_None);
    PointGap4->SetSize(FVector2D(Theme::S4, 0.0f));
    PointRow4->AddChildToHorizontalBox(PointGap4);

    UVerticalBox* QualityColumn = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("QualityColumn"));
    USizeBox* QualityColumnBox = MakeSizeBox(WidgetTree, QualityColumn, 172.0f, Theme::H4);
    PointRow4->AddChildToHorizontalBox(QualityColumnBox)->SetVerticalAlignment(VAlign_Top);
    AddColumnLabel(QualityColumn, FText::FromString(TEXT("QUALITY")));
    QualityColumn->AddChildToVerticalBox(MakeStepper(TEXT("Quality"), 172.0f,
        QualityMinusButton, QualityPlusButton, QualityValueText));

    UVerticalBoxSlot* PointCardSlot = Stack->AddChildToVerticalBox(PointCard);
    PointCardSlot->SetPadding(FMargin(0.0f, Theme::S7, 0.0f, Theme::S1));
}

void UJusyncPathlineWidget::AddSliderRow(UVerticalBox* Parent, const FText& Label,
    const FName SliderName, float Min, float Max, float Step,
    TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutValue)
{
    USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), NAME_None);
    RowBox->SetHeightOverride(Theme::H4);
    Parent->AddChildToVerticalBox(RowBox);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), NAME_None);
    RowBox->AddChild(Row);

    UTextBlock* LabelText = MakeLabel(NAME_None, Label);
    StyleLabel(LabelText, 10.0f, Theme::TextMuted);
    USizeBox* LabelBox = MakeSizeBox(WidgetTree, LabelText, Theme::LabelWidth, -1.0f);
    UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox);
    LabelSlot->SetVerticalAlignment(VAlign_Center);

    USlider* Slider = MakeSlider(SliderName, Min, Max, Step);
    UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
    SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    SliderSlot->SetVerticalAlignment(VAlign_Center);

    OutValue = MakeLabel(NAME_None, FText::GetEmpty());
    StyleLabel(OutValue, 11.0f, Theme::TextPrimary);
    OutValue->SetJustification(ETextJustify::Right);
    USizeBox* ValueBox = MakeSizeBox(WidgetTree, OutValue, Theme::ValueWidth, -1.0f);
    UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(ValueBox);
    ValueSlot->SetVerticalAlignment(VAlign_Center);

    OutSlider = Slider;
}

void UJusyncPathlineWidget::AddColumnLabel(UVerticalBox* Column, const FText& Label)
{
    UTextBlock* LabelText = MakeLabel(NAME_None, Label);
    StyleLabel(LabelText, 10.0f, Theme::TextMuted);
    UVerticalBoxSlot* LabelSlot = Column->AddChildToVerticalBox(LabelText);
    LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 1.0f));
}

UWidget* UJusyncPathlineWidget::MakeStepper(const FName BaseName, float Width,
    TObjectPtr<UButton>& OutMinus, TObjectPtr<UButton>& OutPlus, TObjectPtr<UTextBlock>& OutValue)
{
    UBorder* Well = MakeRoundedBorder(NAME_None, Theme::WellFill, Theme::WellEdge,
        4.0f, 1.0f);
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), NAME_None);
    Well->AddChild(Row);

    const FName MinusName(*FString::Printf(TEXT("%sMinus"), *BaseName.ToString()));
    const FName PlusName(*FString::Printf(TEXT("%sPlus"), *BaseName.ToString()));
    OutMinus = MakeIconButton(WidgetTree, MinusName,
        JusyncHUDIcons::EIcon::Minus,
        LOCTEXT("StepMinusTip", "Step this value down"), 12.0f);
    OutPlus = MakeIconButton(WidgetTree, PlusName,
        JusyncHUDIcons::EIcon::Plus,
        LOCTEXT("StepPlusTip", "Step this value up"), 12.0f);
    OutMinus->SetStyle(MakeButtonStyle(FLinearColor::Transparent,
        FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
        FLinearColor::Transparent, 4.0f));
    OutPlus->SetStyle(MakeButtonStyle(FLinearColor::Transparent,
        FLinearColor(1.0f, 1.0f, 1.0f, 0.08f), FLinearColor(1.0f, 1.0f, 1.0f, 0.14f),
        FLinearColor::Transparent, 4.0f));

    USizeBox* MinusBox = MakeSizeBox(WidgetTree, OutMinus, 24.0f, Theme::H3);
    Row->AddChildToHorizontalBox(MinusBox)->SetVerticalAlignment(VAlign_Center);

    OutValue = MakeLabel(NAME_None, FText::FromString(TEXT("0")));
    StyleLabel(OutValue, 11.0f, Theme::TextPrimary);
    OutValue->SetJustification(ETextJustify::Center);
    UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(OutValue);
    ValueSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ValueSlot->SetHorizontalAlignment(HAlign_Center);
    ValueSlot->SetVerticalAlignment(VAlign_Center);

    USizeBox* PlusBox = MakeSizeBox(WidgetTree, OutPlus, 24.0f, Theme::H3);
    Row->AddChildToHorizontalBox(PlusBox)->SetVerticalAlignment(VAlign_Center);

    return MakeSizeBox(WidgetTree, Well, Width, Theme::H3);
}

UTextBlock* UJusyncPathlineWidget::MakeLabel(const FName Name, const FText& Label)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(Label);
    return Text;
}

UButton* UJusyncPathlineWidget::MakeButton(const FName Name, const FText& Label,
    const FText& Tooltip, TObjectPtr<UTextBlock>* OutCaption, float FontSize)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetToolTipText(Tooltip);
    Button->SetStyle(MakeButtonStyle(Theme::ChipIdle, Theme::ChipHover, Theme::ChipPressed,
        Theme::ChipDisabled, 6.0f));
    Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

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

UBorder* UJusyncPathlineWidget::MakeRoundedBorder(const FName Name, const FLinearColor& Fill,
    const FLinearColor& Outline, float Radius, float OutlineWidth, const FMargin& InPadding)
{
    UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
    Border->SetBrush(FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth));
    Border->SetPadding(InPadding);
    return Border;
}

UButton* UJusyncPathlineWidget::MakeSwitch(const FName Name, const FText& Tooltip,
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

void UJusyncPathlineWidget::SetSwitchState(UButton* Button, UBorder* Thumb, bool bOn)
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

USlider* UJusyncPathlineWidget::MakeSlider(const FName Name, float Min, float Max, float Step)
{
    USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), Name);
    Slider->SetMinValue(Min);
    Slider->SetMaxValue(Max);
    Slider->SetStepSize(Step);
    Slider->SetIndentHandle(false);
    Slider->SetWidgetStyle(MakeSliderStyle());
    return Slider;
}

UComboBoxString* UJusyncPathlineWidget::MakeCombo(const FName Name,
    const TArray<FString>& Options, int32 DefaultIndex)
{
    UComboBoxString* Combo = WidgetTree->ConstructWidget<UComboBoxString>(
        UComboBoxString::StaticClass(), Name);
    Combo->SetWidgetStyle(MakeComboStyle());
    Combo->SetItemStyle(MakeComboItemStyle());
    Combo->SetContentPadding(FMargin(10.0f, 0.0f, 8.0f, 0.0f));
    Combo->SetMaxListHeight(220.0f);
    Combo->SetHasDownArrow(true);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo->ForegroundColor = FSlateColor(Theme::TextPrimary);
    Combo->Font.Size = 10;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    for (const FString& Option : Options)
    {
        Combo->AddOption(Option);
    }
    Combo->SetSelectedIndex(FMath::Clamp(DefaultIndex, 0, FMath::Max(0, Options.Num() - 1)));
    return Combo;
}

int32 UJusyncPathlineWidget::CurrentIntCvar(const TCHAR* Name, int32 Default)
{
    if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        return CVar->GetInt();
    }
    return Default;
}

void UJusyncPathlineWidget::SetIntCvar(const TCHAR* Name, int32 Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByConsole);
    }
}

float UJusyncPathlineWidget::CurrentFloatCvar(const TCHAR* Name, float Default)
{
    if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        return CVar->GetFloat();
    }
    return Default;
}

void UJusyncPathlineWidget::SetFloatCvar(const TCHAR* Name, float Value)
{
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
    {
        CVar->Set(Value, ECVF_SetByConsole);
    }
}

int32 UJusyncPathlineWidget::EffectiveHistorySamples() const
{
    const int32 Override = CurrentIntCvar(TEXT("jusync.Pathlines.HistorySamples"), 0);
    if (Override > 0)
    {
        return Override;
    }
    return Spawner ? Spawner->PathlineHistorySamples : 128;
}

int32 UJusyncPathlineWidget::EffectiveMaxTracks() const
{
    const int32 Override = CurrentIntCvar(TEXT("jusync.Pathlines.MaxTracks"), 0);
    if (Override > 0)
    {
        return Override;
    }
    return Spawner ? Spawner->PathlineMaxTracks : 1000;
}

void UJusyncPathlineWidget::Refresh()
{
    if (!Panel)
    {
        return;
    }

    const bool bOn = Spawner && Spawner->bEnablePathlines;
    const bool bPoints = Spawner && Spawner->bPointCloudsVisible;
    const int32 ActiveCount = (bOn ? 1 : 0) + (bPoints ? 1 : 0);

    if (ActiveText)
    {
        ActiveText->SetText(FText::FromString(FString::Printf(TEXT("%d ACTIVE"), ActiveCount)));
    }
    if (PathlineStatusText)
    {
        PathlineStatusText->SetText(FText::FromString(Spawner
            ? FString::Printf(TEXT("%d TRACKS"), Spawner->PathlineActiveTracks)
            : TEXT("-- TRACKS")));
        PathlineStatusText->SetColorAndOpacity(FSlateColor(bOn ? Theme::TextMuted : Theme::TextMuted * 0.7f));
    }
    if (PointStatusText)
    {
        PointStatusText->SetText(FText::FromString(Spawner
            ? FString::Printf(TEXT("%d ACTORS"), Spawner->ActorsSpawned)
            : TEXT("-- ACTORS")));
        PointStatusText->SetColorAndOpacity(FSlateColor(bPoints ? Theme::TextMuted : Theme::TextMuted * 0.7f));
    }

    SetSwitchState(EnableSwitch, EnableThumb, bOn);
    SetSwitchState(PointsSwitch, PointsThumb, bPoints);

    const int32 History = EffectiveHistorySamples();
    const float Width = Spawner ? Spawner->PathlineRadiusScale : 1.0f;
    const int32 MaxTracks = EffectiveMaxTracks();
    const int32 Expiry = Spawner ? Spawner->PathlineTrackExpirySamples : 64;
    const float PointSize = Spawner ? Spawner->PointCloudSize : 0.04f;
    const bool bColorScalar = Spawner && Spawner->bPathlineColorByScalar;
    const bool bHue = Spawner && Spawner->bPathlineHueFilterEnabled;
    const bool bSquare = !Spawner || Spawner->PointShape == EJUSYNCPointShape::Square;
    const bool bPointHue = Spawner && Spawner->bPointCloudHueFilterEnabled;
    const bool bNormals = Spawner && Spawner->bCalculatePointCloudNormals;
    const int32 Quality = Spawner ? Spawner->PointCloudNormalsQuality : 10;

    int32 HueIndex = 0;
    if (bHue && Spawner)
    {
        const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
        for (int32 Index = 0; Index < Angles.Num(); ++Index)
        {
            if (FJusyncPathlineHUDModel::HueDistance(Spawner->PathlineHue, Angles[Index]) < 1.0f)
            {
                HueIndex = Index + 1;
                break;
            }
        }
    }

    int32 PointHueIndex = 0;
    if (bPointHue && Spawner)
    {
        const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
        for (int32 Index = 0; Index < Angles.Num(); ++Index)
        {
            if (FJusyncPathlineHUDModel::HueDistance(Spawner->PointCloudHue, Angles[Index]) < 1.0f)
            {
                PointHueIndex = Index + 1;
                break;
            }
        }
    }

    if (HistoryValueText)
    {
        HistoryValueText->SetText(FText::AsNumber(History));
    }
    if (WidthValueText)
    {
        WidthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Width)));
    }
    const float PathlineGlow = CurrentFloatCvar(TEXT("jusync.Pathlines.Glow"), 0.1f);
    if (PathlineGlowValueText)
    {
        PathlineGlowValueText->SetText(FText::FromString(PathlineGlow < 10.0f
            ? FString::Printf(TEXT("%.3f"), PathlineGlow)
            : FString::Printf(TEXT("%.0f"), PathlineGlow)));
    }
    if (MaxValueText)
    {
        MaxValueText->SetText(FText::AsNumber(MaxTracks));
    }
    if (ExpiryValueText)
    {
        ExpiryValueText->SetText(FText::AsNumber(Expiry));
    }
    if (PointSizeValueText)
    {
        PointSizeValueText->SetText(FText::FromString(FString::Printf(TEXT("%.3f"), PointSize)));
    }
    const float PointGlow = CurrentFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"), 0.004f);
    if (PointGlowValueText)
    {
        PointGlowValueText->SetText(FText::FromString(PointGlow < 10.0f
            ? FString::Printf(TEXT("%.3f"), PointGlow)
            : FString::Printf(TEXT("%.0f"), PointGlow)));
    }
    if (QualityValueText)
    {
        QualityValueText->SetText(FText::AsNumber(Quality));
        QualityValueText->SetColorAndOpacity(FSlateColor(bNormals ? Theme::TextPrimary : Theme::TextMuted));
    }

    if (HistorySlider)
    {
        HistorySlider->SetIsEnabled(bOn);
        HistorySlider->SetRenderOpacity(bOn ? 1.0f : 0.55f);
    }
    if (WidthSlider)
    {
        WidthSlider->SetIsEnabled(bOn);
        WidthSlider->SetRenderOpacity(bOn ? 1.0f : 0.55f);
    }
    if (PathlineGlowSlider)
    {
        PathlineGlowSlider->SetIsEnabled(bOn);
        PathlineGlowSlider->SetRenderOpacity(bOn ? 1.0f : 0.55f);
    }
    if (PointSizeSlider)
    {
        PointSizeSlider->SetIsEnabled(bPoints);
        PointSizeSlider->SetRenderOpacity(bPoints ? 1.0f : 0.55f);
    }
    if (PointGlowSlider)
    {
        PointGlowSlider->SetIsEnabled(bPoints);
        PointGlowSlider->SetRenderOpacity(bPoints ? 1.0f : 0.55f);
    }
    if (ColorCombo)
    {
        ColorCombo->SetIsEnabled(bOn);
        ColorCombo->SetRenderOpacity(bOn ? 1.0f : 0.62f);
    }
    if (HueCombo)
    {
        HueCombo->SetIsEnabled(bOn);
        HueCombo->SetRenderOpacity(bOn ? 1.0f : 0.62f);
    }
    if (ScalingCombo)
    {
        ScalingCombo->SetIsEnabled(bPoints);
        ScalingCombo->SetRenderOpacity(bPoints ? 1.0f : 0.62f);
    }
    if (OrientationCombo)
    {
        OrientationCombo->SetIsEnabled(bPoints);
        OrientationCombo->SetRenderOpacity(bPoints ? 1.0f : 0.62f);
    }
    if (PointHueCombo)
    {
        PointHueCombo->SetIsEnabled(bPoints);
        PointHueCombo->SetRenderOpacity(bPoints ? 1.0f : 0.62f);
    }
    if (NormalsSwitch)
    {
        NormalsSwitch->SetIsEnabled(bPoints);
        NormalsSwitch->SetRenderOpacity(bPoints ? 1.0f : 0.45f);
    }
    if (ShapeSquareButton)
    {
        ShapeSquareButton->SetIsEnabled(bPoints);
        ShapeSquareButton->SetBackgroundColor(bSquare ? Theme::Accent : Theme::ChipIdle);
        ShapeSquareButton->SetRenderOpacity(bPoints ? 1.0f : 0.45f);
    }
    if (ShapeCircleButton)
    {
        ShapeCircleButton->SetIsEnabled(bPoints);
        ShapeCircleButton->SetBackgroundColor(bSquare ? Theme::ChipIdle : Theme::Accent);
        ShapeCircleButton->SetRenderOpacity(bPoints ? 1.0f : 0.45f);
    }

    const float PathlineControlOpacity = bOn ? 1.0f : 0.45f;
    if (MaxMinusButton)
    {
        MaxMinusButton->SetIsEnabled(bOn);
        MaxMinusButton->SetRenderOpacity(PathlineControlOpacity);
    }
    if (MaxPlusButton)
    {
        MaxPlusButton->SetIsEnabled(bOn);
        MaxPlusButton->SetRenderOpacity(PathlineControlOpacity);
    }
    if (ExpiryMinusButton)
    {
        ExpiryMinusButton->SetIsEnabled(bOn);
        ExpiryMinusButton->SetRenderOpacity(PathlineControlOpacity);
    }
    if (ExpiryPlusButton)
    {
        ExpiryPlusButton->SetIsEnabled(bOn);
        ExpiryPlusButton->SetRenderOpacity(PathlineControlOpacity);
    }
    if (QualityMinusButton)
    {
        QualityMinusButton->SetIsEnabled(bPoints);
        QualityMinusButton->SetRenderOpacity(bPoints ? 1.0f : 0.45f);
    }
    if (QualityPlusButton)
    {
        QualityPlusButton->SetIsEnabled(bPoints);
        QualityPlusButton->SetRenderOpacity(bPoints ? 1.0f : 0.45f);
    }

    SetSwitchState(NormalsSwitch, NormalsThumb, bNormals);

    bApplyingFromCode = true;
    if (HistorySlider)
    {
        HistorySlider->SetValue(static_cast<float>(History));
    }
    if (WidthSlider)
    {
        WidthSlider->SetValue(Width);
    }
    if (PathlineGlowSlider)
    {
        PathlineGlowSlider->SetValue(PathlineGlowToSlider(PathlineGlow));
    }
    if (PointSizeSlider)
    {
        PointSizeSlider->SetValue(PointSize);
    }
    if (PointGlowSlider)
    {
        PointGlowSlider->SetValue(PointGlowToSlider(PointGlow));
    }
    if (ColorCombo)
    {
        ColorCombo->SetSelectedIndex(bColorScalar ? 0 : 1);
    }
    if (HueCombo)
    {
        HueCombo->SetSelectedIndex(HueIndex);
    }
    if (ScalingCombo)
    {
        ScalingCombo->SetSelectedIndex(Spawner ? FMath::Clamp(
            static_cast<int32>(Spawner->PointScaling), 0, 3) : 1);
    }
    if (OrientationCombo)
    {
        OrientationCombo->SetSelectedIndex(Spawner ? FMath::Clamp(
            static_cast<int32>(Spawner->PointOrientation), 0, 1) : 0);
    }
    if (PointHueCombo)
    {
        PointHueCombo->SetSelectedIndex(PointHueIndex);
    }
    bApplyingFromCode = false;
}

void UJusyncPathlineWidget::HandleEnableClicked()
{
    if (Spawner)
    {
        Spawner->bEnablePathlines = !Spawner->bEnablePathlines;
        Refresh();
    }
}

void UJusyncPathlineWidget::HandlePointsEnableClicked()
{
    if (Spawner)
    {
        Spawner->SetPointCloudsVisible(!Spawner->bPointCloudsVisible);
        Refresh();
    }
}

void UJusyncPathlineWidget::HandleHistoryChanged(float Value)
{
    if (bApplyingFromCode || !Spawner)
    {
        return;
    }
    const int32 NewValue = FMath::Clamp(FMath::RoundToInt(Value), 2, 512);
    Spawner->PathlineHistorySamples = NewValue;
    SetIntCvar(TEXT("jusync.Pathlines.HistorySamples"), NewValue);
    Refresh();
}

void UJusyncPathlineWidget::HandleWidthChanged(float Value)
{
    if (bApplyingFromCode || !Spawner)
    {
        return;
    }
    Spawner->PathlineRadiusScale = FMath::Clamp(Value, 0.1f, 5.0f);
    Refresh();
}

void UJusyncPathlineWidget::HandlePathlineGlowChanged(float Value)
{
    if (bApplyingFromCode)
    {
        return;
    }

    SetFloatCvar(TEXT("jusync.Pathlines.Glow"), PathlineGlowFromSlider(Value));
    Refresh();
}

void UJusyncPathlineWidget::SyncPointCloudFixedSizeMode()
{
    if (!Spawner)
    {
        return;
    }

    const bool bFixedScreenSize =
        Spawner->PointScaling == EJUSYNCPointScaling::FixedScreenSize;
    if (Spawner->bCloudFixedSize != bFixedScreenSize)
    {
        Spawner->bCloudFixedSize = bFixedScreenSize;
        Spawner->ApplyMorphSettings();
    }
}

void UJusyncPathlineWidget::HandlePointSizeChanged(float Value)
{
    if (bApplyingFromCode || !Spawner)
    {
        return;
    }
    Spawner->PointCloudSize = FMath::Clamp(Value, 0.001f, 1.0f);
    SyncPointCloudFixedSizeMode();
    Spawner->ApplyPointCloudDisplaySettings();
    Refresh();
}

void UJusyncPathlineWidget::HandlePointGlowChanged(float Value)
{
    if (bApplyingFromCode)
    {
        return;
    }

    const float Glow = PointGlowFromSlider(FJusyncPathlineHUDModel::ClampPointGlow(Value));
    SetFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"), Glow);

    if (Spawner)
    {
        Spawner->ApplyPointCloudDisplaySettings();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleColorSelected(FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || SelectionType == ESelectInfo::Direct || !Spawner || !ColorCombo)
    {
        return;
    }
    Spawner->bPathlineColorByScalar = ColorCombo->GetSelectedIndex() == 0;
    Refresh();
}

void UJusyncPathlineWidget::HandleHueSelected(FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || SelectionType == ESelectInfo::Direct || !Spawner || !HueCombo)
    {
        return;
    }

    const int32 Index = HueCombo->GetSelectedIndex();
    if (Index <= 0)
    {
        Spawner->bPathlineHueFilterEnabled = false;
    }
    else
    {
        const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
        if (Angles.IsValidIndex(Index - 1))
        {
            Spawner->PathlineHue = Angles[Index - 1];
            Spawner->PathlineHueTolerance = 30.0f;
            Spawner->bPathlineHueFilterEnabled = true;
        }
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleShapeSquareClicked()
{
    if (Spawner)
    {
        Spawner->PointShape = EJUSYNCPointShape::Square;
        Spawner->ApplyPointCloudDisplaySettings();
        Refresh();
    }
}

void UJusyncPathlineWidget::HandleShapeCircleClicked()
{
    if (Spawner)
    {
        Spawner->PointShape = EJUSYNCPointShape::Circle;
        Spawner->ApplyPointCloudDisplaySettings();
        Refresh();
    }
}

void UJusyncPathlineWidget::HandleScalingSelected(FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || SelectionType == ESelectInfo::Direct || !Spawner || !ScalingCombo)
    {
        return;
    }
    Spawner->PointScaling = static_cast<EJUSYNCPointScaling>(
        FMath::Clamp(ScalingCombo->GetSelectedIndex(), 0, 3));
    SyncPointCloudFixedSizeMode();
    Spawner->ApplyPointCloudDisplaySettings();
    Refresh();
}

void UJusyncPathlineWidget::HandleOrientationSelected(FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || SelectionType == ESelectInfo::Direct
        || !Spawner || !OrientationCombo)
    {
        return;
    }
    Spawner->PointOrientation = static_cast<EJUSYNCPointOrientation>(
        FMath::Clamp(OrientationCombo->GetSelectedIndex(), 0, 1));
    Spawner->ApplyPointCloudDisplaySettings();
    Refresh();
}

void UJusyncPathlineWidget::HandlePointHueSelected(FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    if (bApplyingFromCode || SelectionType == ESelectInfo::Direct || !Spawner || !PointHueCombo)
    {
        return;
    }

    const int32 Index = PointHueCombo->GetSelectedIndex();
    if (Index <= 0)
    {
        Spawner->bPointCloudHueFilterEnabled = false;
    }
    else
    {
        const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
        if (Angles.IsValidIndex(Index - 1))
        {
            Spawner->PointCloudHue = Angles[Index - 1];
            Spawner->PointCloudHueTolerance = 30.0f;
            Spawner->bPointCloudHueFilterEnabled = true;
        }
    }

    Spawner->UpdatePointCloudHueFilter();
    Refresh();
}

void UJusyncPathlineWidget::HandleNormalsClicked()
{
    if (Spawner)
    {
        Spawner->bCalculatePointCloudNormals = !Spawner->bCalculatePointCloudNormals;
        Spawner->ApplyPointCloudDisplaySettings(true);
        Refresh();
    }
}

void UJusyncPathlineWidget::StepMaxBy(int32 Direction)
{
    SetIntCvar(TEXT("jusync.Pathlines.MaxTracks"),
        FJusyncPathlineHUDModel::StepTrackPreset(EffectiveMaxTracks(), Direction));
    Refresh();
}

void UJusyncPathlineWidget::StepExpiryBy(int32 Direction)
{
    if (Spawner)
    {
        Spawner->PathlineTrackExpirySamples = FMath::Clamp(
            Spawner->PathlineTrackExpirySamples + Direction * 4, 1, 512);
        Refresh();
    }
}

void UJusyncPathlineWidget::StepQualityBy(int32 Direction)
{
    if (Spawner)
    {
        Spawner->PointCloudNormalsQuality = FMath::Clamp(
            Spawner->PointCloudNormalsQuality + Direction, 1, 100);
        Spawner->ApplyPointCloudDisplaySettings(true);
        Refresh();
    }
}

#undef LOCTEXT_NAMESPACE
