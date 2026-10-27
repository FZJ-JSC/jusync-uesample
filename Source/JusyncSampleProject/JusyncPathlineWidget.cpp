#include "JusyncPathlineWidget.h"

#include "JusyncPathlineHUDModel.h"
#include "JusyncPlaybackHUDPlayerController.h"
#include "JUSYNCFileSpawnerActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Styling/SlateTypes.h"

#define LOCTEXT_NAMESPACE "JusyncPathlineWidget"

namespace
{
    /** Same visual language as the playback panel; see its note on brushes. */
    namespace Theme
    {
        const FLinearColor PanelEdge(1.0f, 1.0f, 1.0f, 0.10f);
        const FLinearColor PanelFill(0.035f, 0.040f, 0.050f, 0.88f);
        const FLinearColor Divider(1.0f, 1.0f, 1.0f, 0.09f);

        const FLinearColor TextPrimary(0.94f, 0.95f, 0.97f, 1.0f);
        const FLinearColor TextMuted(0.60f, 0.64f, 0.70f, 1.0f);
        const FLinearColor TextShadow(0.0f, 0.0f, 0.0f, 0.65f);

        const FLinearColor Live(0.35f, 0.85f, 0.45f, 1.0f);
        const FLinearColor Paused(1.0f, 0.72f, 0.24f, 1.0f);

        const FLinearColor ChipIdle(0.150f, 0.170f, 0.205f, 0.92f);
        const FLinearColor ChipHover(0.220f, 0.250f, 0.295f, 1.00f);
        const FLinearColor ChipPressed(0.200f, 0.450f, 0.850f, 1.00f);
        const FLinearColor ChipDisabled(0.100f, 0.110f, 0.130f, 0.45f);

        // Original narrow box: rows stack vertically (the panel grows TALLER,
        // never wider) - see the POINTS section, whose SIZE=/GLOW= inputs got
        // their own row instead of overflowing the right edge.
        constexpr float PanelWidth = 480.0f;
        constexpr float TitleSize = 11.0f;
        constexpr float StatusSize = 13.0f;
        constexpr float ChipSize = 12.0f;
        constexpr float HintSize = 10.0f;
        constexpr float StepperInputWidth = 68.0f;
        const FMargin ChipPadding(10.0f, 4.0f);
        const FMargin MiniPadding(5.0f, 1.0f);
        const FMargin HueChipPadding(12.0f, 4.0f);
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

    FButtonStyle MakeChipStyle()
    {
        FButtonStyle Style = FButtonStyle::GetDefault();
        Style.Normal.TintColor = FSlateColor(Theme::ChipIdle);
        Style.Hovered.TintColor = FSlateColor(Theme::ChipHover);
        Style.Pressed.TintColor = FSlateColor(Theme::ChipPressed);
        Style.Disabled.TintColor = FSlateColor(Theme::ChipDisabled);
        return Style;
    }

    /** Hue chips paint their own colour, so the state tints stay light. */
    FButtonStyle MakeHueChipStyle()
    {
        FButtonStyle Style = FButtonStyle::GetDefault();
        Style.Normal.TintColor = FSlateColor(FLinearColor::White);
        Style.Hovered.TintColor = FSlateColor(FLinearColor(1.25f, 1.25f, 1.25f));
        Style.Pressed.TintColor = FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f));
        Style.Disabled.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
        return Style;
    }
}

void UJusyncPathlineWidget::BindSpawner(AJUSYNCFileSpawnerActor* InSpawner)
{
    Spawner = InSpawner;
    EnsureLayoutBuilt();
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

    EnableButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleEnableClicked);
    ColorButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleColorClicked);
    HueButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueToggle);
    ToleranceMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleToleranceMinus);
    TolerancePlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleTolerancePlus);

    RadiusMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleRadiusMinus);
    RadiusPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleRadiusPlus);
    PointSizeMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointSizeMinus);
    PointSizePlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointSizePlus);
    EmissiveMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleEmissiveMinus);
    EmissivePlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleEmissivePlus);
    SidesMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleSidesMinus);
    SidesPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleSidesPlus);

    PointsEnableButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointsEnableClicked);
    PointHueButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueToggle);
    PointToleranceMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointToleranceMinus);
    PointTolerancePlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointTolerancePlus);
    PointGlowMinusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointGlowMinus);
    PointGlowPlusButton->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointGlowPlus);

    // OnClicked is a dynamic-only delegate in 5.8: one UFUNCTION per chip.
    if (HueChipButtons.Num() > 0) HueChipButtons[0]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipRed);
    if (HueChipButtons.Num() > 1) HueChipButtons[1]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipYellow);
    if (HueChipButtons.Num() > 2) HueChipButtons[2]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipGreen);
    if (HueChipButtons.Num() > 3) HueChipButtons[3]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipCyan);
    if (HueChipButtons.Num() > 4) HueChipButtons[4]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipBlue);
    if (HueChipButtons.Num() > 5) HueChipButtons[5]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandleHueChipMagenta);

    if (PointHueChipButtons.Num() > 0) PointHueChipButtons[0]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipRed);
    if (PointHueChipButtons.Num() > 1) PointHueChipButtons[1]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipYellow);
    if (PointHueChipButtons.Num() > 2) PointHueChipButtons[2]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipGreen);
    if (PointHueChipButtons.Num() > 3) PointHueChipButtons[3]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipCyan);
    if (PointHueChipButtons.Num() > 4) PointHueChipButtons[4]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipBlue);
    if (PointHueChipButtons.Num() > 5) PointHueChipButtons[5]->OnClicked.AddDynamic(this, &UJusyncPathlineWidget::HandlePointHueChipMagenta);

    Refresh();
}

void UJusyncPathlineWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureLayoutBuilt();
}

void UJusyncPathlineWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void UJusyncPathlineWidget::BuildLayout()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Edge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
    Edge->SetBrushColor(Theme::PanelEdge);
    Edge->SetPadding(FMargin(1.0f));

    // Top-right, so the panel never stacks on the bottom-centre transport.
    UCanvasPanelSlot* EdgeSlot = Root->AddChildToCanvas(Edge);
    EdgeSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
    EdgeSlot->SetAlignment(FVector2D(1.0f, 0.0f));
    EdgeSlot->SetPosition(FVector2D(-20.0f, 20.0f));
    EdgeSlot->SetAutoSize(true);

    Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
    Panel->SetBrushColor(Theme::PanelFill);
    Panel->SetPadding(FMargin(14.0f, 10.0f, 14.0f, 10.0f));
    Edge->AddChild(Panel);

    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Width"));
    Width->SetWidthOverride(Theme::PanelWidth);
    Panel->AddChild(Width);

    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("Stack"));
    Width->AddChild(Stack);

    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), NAME_None);
    Stack->AddChildToVerticalBox(Header);

    UTextBlock* Title = MakeLabel(NAME_None, LOCTEXT("PanelTitle", "PATHLINES"));
    StyleLabel(Title, Theme::TitleSize, Theme::TextMuted);
    Header->AddChildToHorizontalBox(Title);

    UTextBlock* Hint = MakeLabel(NAME_None, LOCTEXT("PanelHint", "P hides  ·  click a colour to filter"));
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
    RuleSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 7.0f));

    StatusText = MakeLabel(TEXT("StatusText"), FText::GetEmpty());
    StyleLabel(StatusText, Theme::StatusSize, Theme::TextPrimary);
    Stack->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));

    // Row: state toggles.
    UHorizontalBox* Toggles = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("Toggles"));
    Stack->AddChildToVerticalBox(Toggles)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    EnableButton = MakeButton(TEXT("EnableButton"),
        LOCTEXT("EnableOn", "TRAILS ON"),
        LOCTEXT("EnableTip", "Show or hide the particle trail tubes"),
        &EnableCaption);
    ColorButton = MakeButton(TEXT("ColorButton"),
        FJusyncPathlineHUDModel::MakeColorLabel(true),
        LOCTEXT("ColorTip", "Colour trails per sample by the scalar attribute, or flat"),
        &ColorCaption);
    HueButton = MakeButton(TEXT("HueButton"),
        FJusyncPathlineHUDModel::MakeHueLabel(false, 0.0f, 30.0f),
        LOCTEXT("HueTip", "Turn the colour filter on or off; the chips below pick the colour"),
        &HueCaption);

    auto AddChip = [Toggles](UButton* Chip)
    {
        UHorizontalBoxSlot* ChipSlot = Toggles->AddChildToHorizontalBox(Chip);
        ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
        ChipSlot->SetVerticalAlignment(VAlign_Center);
    };
    AddChip(EnableButton);
    AddChip(ColorButton);
    AddChip(HueButton);

    // Row: colour picker. Each chip IS the colour it filters for; clicking
    // again clears back to no filter.
    UHorizontalBox* HueRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("HueRow"));
    Stack->AddChildToVerticalBox(HueRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    const TArray<FLinearColor>& ChipColors = FJusyncPathlineHUDModel::HueChipColors();
    for (int32 Index = 0; Index < ChipColors.Num(); ++Index)
    {
        static const TCHAR* ChipGlyphs[] = { TEXT("R"), TEXT("Y"), TEXT("G"),
                                             TEXT("C"), TEXT("B"), TEXT("M") };
        UButton* Chip = MakeButton(FName(*FString::Printf(TEXT("HueChip%d"), Index)),
            FText::FromString(ChipGlyphs[Index % UE_ARRAY_COUNT(ChipGlyphs)]),
            LOCTEXT("HueChipTip", "Filter trails to this colour (click again to clear)"),
            nullptr);
        Chip->SetStyle(MakeHueChipStyle());
        Chip->SetBackgroundColor(ChipColors[Index]);
        if (UTextBlock* ChipCaption = Cast<UTextBlock>(Chip->GetChildAt(0)))
        {
            ChipCaption->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
        }
        HueChipButtons.Add(Chip);

        UHorizontalBoxSlot* ChipSlot = HueRow->AddChildToHorizontalBox(Chip);
        ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
        ChipSlot->SetVerticalAlignment(VAlign_Center);
    }

    ToleranceMinusButton = MakeButton(TEXT("TolMinus"), FText::FromString(TEXT("-")),
        LOCTEXT("TolMinusTip", "Wider colour match (adds tolerance in degrees)"));
    TolerancePlusButton = MakeButton(TEXT("TolPlus"), FText::FromString(TEXT("+")),
        LOCTEXT("TolPlusTip", "Narrower colour match (removes tolerance in degrees)"));
    // NOTE on direction: minus WIDENS (see handler), plus NARROWS - the
    // caption reads "± 30" so more tolerance = more colours.
    ToleranceCaption = MakeLabel(TEXT("ToleranceCaption"),
        FJusyncPathlineHUDModel::MakeHueLabel(true, 0.0f, 30.0f));
    StyleLabel(ToleranceCaption, Theme::ChipSize, Theme::TextPrimary);
    AddStepperWidgets(HueRow, ToleranceMinusButton, ToleranceCaption, TolerancePlusButton);

    // Row: budget + thickness typed inputs (values shown live in the boxes).
    UHorizontalBox* Budget = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("Budget"));
    Stack->AddChildToVerticalBox(Budget);

    // Typed overrides: type an exact value, press Enter or click away.
    // With stepper buttons the row reads "R= [box] [-] [+]".
    auto AddInput = [this](UHorizontalBox* Row, TObjectPtr<UEditableTextBox>& Input,
        const FName Name, const FText& Label, const FText& Tooltip,
        TObjectPtr<UButton>* Minus = nullptr, TObjectPtr<UButton>* Plus = nullptr,
        float InputWidth = 84.0f)
    {
        UTextBlock* Caption = MakeLabel(NAME_None, Label);
        StyleLabel(Caption, Theme::ChipSize, Theme::TextMuted);
        UHorizontalBoxSlot* CaptionSlot = Row->AddChildToHorizontalBox(Caption);
        CaptionSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));
        CaptionSlot->SetVerticalAlignment(VAlign_Center);

        Input = WidgetTree->ConstructWidget<UEditableTextBox>(
            UEditableTextBox::StaticClass(), Name);
        Input->SetToolTipText(Tooltip);
        Input->SetForegroundColor(Theme::TextPrimary);
        Input->SetJustification(ETextJustify::Right);

        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(
            USizeBox::StaticClass(), NAME_None);
        Box->SetWidthOverride(InputWidth);
        Box->AddChild(Input);
        UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Box);
        Slot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
        Slot->SetVerticalAlignment(VAlign_Center);

        auto AddMini = [this, Row](TObjectPtr<UButton>& Button, const FName ButtonName,
            const TCHAR* Glyph, const FText& MiniTooltip)
        {
            Button = MakeButton(ButtonName, FText::FromString(Glyph), MiniTooltip, nullptr, true);
            UHorizontalBoxSlot* MiniSlot = Row->AddChildToHorizontalBox(Button);
            MiniSlot->SetPadding(FMargin(2.0f, 0.0f, 0.0f, 0.0f));
            MiniSlot->SetVerticalAlignment(VAlign_Center);
        };
        if (Minus)
        {
            AddMini(*Minus, FName(*FString::Printf(TEXT("%sMinus"), *Name.ToString())),
                TEXT("-"), LOCTEXT("StepMinusTip", "Step this value down"));
        }
        if (Plus)
        {
            AddMini(*Plus, FName(*FString::Printf(TEXT("%sPlus"), *Name.ToString())),
                TEXT("+"), LOCTEXT("StepPlusTip", "Step this value up"));
        }
    };

    AddInput(Budget, MaxInput, TEXT("MaxInput"), LOCTEXT("MaxInputLabel", "MAX="),
        LOCTEXT("MaxInputTip", "Exact track budget (1 to 1000). Enter to apply."));
    MaxInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandleMaxInputCommitted);

    AddInput(Budget, RadiusInput, TEXT("RadiusInput"), LOCTEXT("RadiusInputLabel", "R="),
        LOCTEXT("RadiusInputTip", "Exact trail radius in cm (0.02 to 50). Enter to apply."),
        &RadiusMinusButton, &RadiusPlusButton, Theme::StepperInputWidth);
    RadiusInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandleRadiusInputCommitted);

    // Row: trail emission + tube sides. (Point size/glow live in the POINTS
    // section below, together with their own colour filter.)
    UHorizontalBox* Visual = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("Visual"));
    Stack->AddChildToVerticalBox(Visual)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

    AddInput(Visual, EmissiveInput, TEXT("EmissiveInput"), LOCTEXT("EmissiveLabel", "EM="),
        LOCTEXT("EmissiveTip", "Trail glow strength (0 to 4). Enter to apply."),
        &EmissiveMinusButton, &EmissivePlusButton, Theme::StepperInputWidth);
    EmissiveInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandleEmissiveInputCommitted);

    AddInput(Visual, SidesInput, TEXT("SidesInput"), LOCTEXT("SidesLabel", "SIDES="),
        LOCTEXT("SidesTip", "Tube sides per trail (3 to 16). Enter to apply."),
        &SidesMinusButton, &SidesPlusButton, Theme::StepperInputWidth);
    SidesInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandleSidesInputCommitted);

    // ---- POINTS section: same control language, aimed at the LiDAR clouds.
    UBorder* PointsRule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), NAME_None);
    PointsRule->SetBrushColor(Theme::Divider);
    USizeBox* PointsRuleHeight = WidgetTree->ConstructWidget<USizeBox>(
        USizeBox::StaticClass(), NAME_None);
    PointsRuleHeight->SetHeightOverride(1.0f);
    PointsRuleHeight->AddChild(PointsRule);
    UVerticalBoxSlot* PointsRuleSlot = Stack->AddChildToVerticalBox(PointsRuleHeight);
    PointsRuleSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 5.0f));

    UTextBlock* PointsTitle = MakeLabel(TEXT("PointsTitle"), LOCTEXT("PointsTitle", "POINTS"));
    StyleLabel(PointsTitle, Theme::TitleSize, Theme::TextMuted);
    Stack->AddChildToVerticalBox(PointsTitle)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));

    // Row: cloud visibility, point hue toggle, point size and glow inputs.
    UHorizontalBox* PointsRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointsRow"));
    Stack->AddChildToVerticalBox(PointsRow)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    PointsEnableButton = MakeButton(TEXT("PointsEnableButton"),
        LOCTEXT("PointsOn", "POINTS ON"),
        LOCTEXT("PointsOnTip", "Show or hide the streamed point clouds (applies to future frames too)"),
        &PointsEnableCaption);
    UHorizontalBoxSlot* PointsEnableSlot = PointsRow->AddChildToHorizontalBox(PointsEnableButton);
    PointsEnableSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
    PointsEnableSlot->SetVerticalAlignment(VAlign_Center);

    PointHueButton = MakeButton(TEXT("PointHueButton"),
        FJusyncPathlineHUDModel::MakeHueLabel(false, 0.0f, 30.0f),
        LOCTEXT("PointHueTip", "Colour-filter the points; chips below pick the colour"),
        &PointHueCaption);
    UHorizontalBoxSlot* PointHueSlot = PointsRow->AddChildToHorizontalBox(PointHueButton);
    PointHueSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
    PointHueSlot->SetVerticalAlignment(VAlign_Center);

    // Row: point size and glow inputs, on their own line so nothing clips.
    UHorizontalBox* PointsVisual = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointsVisual"));
    Stack->AddChildToVerticalBox(PointsVisual)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

    AddInput(PointsVisual, PointSizeInput, TEXT("PointSizeInput"), LOCTEXT("PointSizeLabel", "SIZE="),
        LOCTEXT("PointSizeTip", "Point size (screen units, 0.01 to 10). Enter to apply."),
        &PointSizeMinusButton, &PointSizePlusButton, Theme::StepperInputWidth);
    PointSizeInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandlePointSizeInputCommitted);

    AddInput(PointsVisual, PointGlowInput, TEXT("PointGlowInput"), LOCTEXT("PointGlowLabel", "GLOW="),
        LOCTEXT("PointGlowTip", "Point self-glow (0 to 1). Enter to apply."),
        &PointGlowMinusButton, &PointGlowPlusButton, Theme::StepperInputWidth);
    PointGlowInput->OnTextCommitted.AddDynamic(this, &UJusyncPathlineWidget::HandlePointGlowInputCommitted);

    // Row: point colour picker - mirrors the trail chips above.
    UHorizontalBox* PointHueRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("PointHueRow"));
    Stack->AddChildToVerticalBox(PointHueRow);

    for (int32 Index = 0; Index < ChipColors.Num(); ++Index)
    {
        static const TCHAR* PChipGlyphs[] = { TEXT("R"), TEXT("Y"), TEXT("G"),
                                              TEXT("C"), TEXT("B"), TEXT("M") };
        UButton* Chip = MakeButton(FName(*FString::Printf(TEXT("PointHueChip%d"), Index)),
            FText::FromString(PChipGlyphs[Index % UE_ARRAY_COUNT(PChipGlyphs)]),
            LOCTEXT("PointHueChipTip", "Filter points to this colour (click again to clear)"),
            nullptr);
        Chip->SetStyle(MakeHueChipStyle());
        Chip->SetBackgroundColor(ChipColors[Index]);
        if (UTextBlock* ChipCaption = Cast<UTextBlock>(Chip->GetChildAt(0)))
        {
            ChipCaption->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
        }
        PointHueChipButtons.Add(Chip);

        UHorizontalBoxSlot* ChipSlot = PointHueRow->AddChildToHorizontalBox(Chip);
        ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
        ChipSlot->SetVerticalAlignment(VAlign_Center);
    }

    PointToleranceMinusButton = MakeButton(TEXT("PointTolMinus"), FText::FromString(TEXT("-")),
        LOCTEXT("PointTolMinusTip", "Wider point colour match (adds tolerance in degrees)"));
    PointTolerancePlusButton = MakeButton(TEXT("PointTolPlus"), FText::FromString(TEXT("+")),
        LOCTEXT("PointTolPlusTip", "Narrower point colour match (removes tolerance in degrees)"));
    PointToleranceCaption = MakeLabel(TEXT("PointToleranceCaption"),
        FJusyncPathlineHUDModel::MakeHueLabel(true, 0.0f, 30.0f));
    StyleLabel(PointToleranceCaption, Theme::ChipSize, Theme::TextPrimary);
    AddStepperWidgets(PointHueRow, PointToleranceMinusButton, PointToleranceCaption,
        PointTolerancePlusButton);
}

void UJusyncPathlineWidget::AddStepperWidgets(UHorizontalBox* Row, UButton* MinusButton,
    UTextBlock* Caption, UButton* PlusButton)
{
    auto Pad = [Row](UWidget* Widget, const FMargin& InMargin)
    {
        UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget);
        Slot->SetPadding(InMargin);
        Slot->SetVerticalAlignment(VAlign_Center);
    };

    Pad(Caption, FMargin(6.0f, 0.0f, 2.0f, 0.0f));
    Pad(MinusButton, FMargin(0.0f, 0.0f, 2.0f, 0.0f));
    Pad(PlusButton, FMargin(0.0f, 0.0f, 12.0f, 0.0f));
}

UTextBlock* UJusyncPathlineWidget::MakeLabel(const FName Name, const FText& Label)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(Label);
    return Text;
}

UButton* UJusyncPathlineWidget::MakeButton(const FName Name, const FText& Label, const FText& Tooltip,
    TObjectPtr<UTextBlock>* OutCaption, bool bMini)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    Button->SetToolTipText(Tooltip);
    Button->SetStyle(MakeChipStyle());
    Button->SetClickMethod(EButtonClickMethod::Type::PreciseClick);

    // NAME_None on purpose (see UJusyncPlaybackWidget::MakeButton): reusing the
    // button's name for the caption would replace it in the WidgetTree.
    UTextBlock* Caption = MakeLabel(NAME_None, Label);
    StyleLabel(Caption, Theme::ChipSize, Theme::TextPrimary);
    Caption->SetJustification(ETextJustify::Center);
    Button->AddChild(Caption);
    if (OutCaption)
    {
        *OutCaption = Caption;
    }

    if (UButtonSlot* CaptionSlot = Cast<UButtonSlot>(Caption->Slot))
    {
        CaptionSlot->SetPadding(bMini ? Theme::MiniPadding : Theme::ChipPadding);
        CaptionSlot->SetHorizontalAlignment(HAlign_Center);
        CaptionSlot->SetVerticalAlignment(VAlign_Center);
    }
    return Button;
}

int32 UJusyncPathlineWidget::EffectiveMaxTracks() const
{
    if (const IConsoleVariable* CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.MaxTracks")))
    {
        const int32 Override = CVar->GetInt();
        if (Override > 0)
        {
            return Override;
        }
    }
    return Spawner ? Spawner->PathlineMaxTracks : 0;
}

float UJusyncPathlineWidget::EffectiveRadius() const
{
    if (const IConsoleVariable* CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.Radius")))
    {
        const float Override = CVar->GetFloat();
        if (Override > 0.0f)
        {
            return Override;
        }
    }
    return Spawner ? Spawner->PathlineRadius : 0.0f;
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
        CVar->Set(Value);
    }
}

FReply UJusyncPathlineWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Slate routes keys along the FOCUSED widget's path, so while the caret
    // sits in one of these text boxes the player controller's InputKey never
    // runs and Tab/P looked dead ("can't use both panels at once"). Handle
    // the toggle keys here, before the focus system eats Tab for traversal.
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

void UJusyncPathlineWidget::ReleaseInputFocus()
{
    // Enter means "applied"; give the keyboard back to the game so the next
    // P/Tab press reaches the controller instead of being typed into the box.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    }
}

void UJusyncPathlineWidget::StepRadiusBy(int32 Direction)
{
    SetFloatCvar(TEXT("jusync.Pathlines.Radius"),
        FJusyncPathlineHUDModel::StepRadius(EffectiveRadius(), Direction));
    Refresh();
}

void UJusyncPathlineWidget::StepPointSizeBy(int32 Direction)
{
    SetFloatCvar(TEXT("jusync.Morph.CloudScreenSize"), FJusyncPathlineHUDModel::StepPointSize(
        CurrentFloatCvar(TEXT("jusync.Morph.CloudScreenSize"), 2.7f), Direction));
    Refresh();
}

void UJusyncPathlineWidget::StepEmissiveBy(int32 Direction)
{
    if (Spawner)
    {
        Spawner->PathlineEmissiveStrength =
            FJusyncPathlineHUDModel::StepEmissive(Spawner->PathlineEmissiveStrength, Direction);
    }
    Refresh();
}

void UJusyncPathlineWidget::StepSidesBy(int32 Direction)
{
    if (IConsoleVariable* CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.TubeSides")))
    {
        CVar->Set(FJusyncPathlineHUDModel::StepTubeSides(CVar->GetInt(), Direction));
    }
    Refresh();
}

void UJusyncPathlineWidget::Refresh()
{
    if (!Panel)
    {
        return;
    }

    const bool bOn = Spawner && Spawner->bEnablePathlines;
    const int32 Active = Spawner ? Spawner->PathlineActiveTracks : 0;
    const int32 VisibleTracks = Spawner ? Spawner->PathlineVisibleTracks : 0;
    const int64 Nodes = Spawner ? Spawner->PathlineNodeCount : 0;

    StatusText->SetText(FJusyncPathlineHUDModel::MakeStatusLabel(Active, VisibleTracks, Nodes));
    StatusText->SetColorAndOpacity(FSlateColor(
        !Spawner ? Theme::TextMuted : (bOn ? Theme::Live : Theme::Paused)));

    EnableButton->SetIsEnabled(Spawner != nullptr);
    EnableCaption->SetText(bOn ? LOCTEXT("TrailsOn", "TRAILS ON")
                               : LOCTEXT("TrailsOff", "TRAILS OFF"));
    EnableButton->SetColorAndOpacity(bOn ? FLinearColor::White : Theme::Paused);

    const bool bColorScalar = Spawner && Spawner->bPathlineColorByScalar;
    ColorButton->SetIsEnabled(bOn);
    ColorButton->SetRenderOpacity(bOn ? 1.0f : 0.45f);
    ColorCaption->SetText(FJusyncPathlineHUDModel::MakeColorLabel(bColorScalar));

    const bool bHue = Spawner && Spawner->bPathlineHueFilterEnabled;
    const float Hue = Spawner ? Spawner->PathlineHue : 0.0f;
    const float Tolerance = Spawner ? Spawner->PathlineHueTolerance : 30.0f;

    HueButton->SetIsEnabled(bOn);
    HueButton->SetRenderOpacity(bOn ? 1.0f : 0.45f);
    HueCaption->SetText(FJusyncPathlineHUDModel::MakeHueLabel(bHue, Hue, Tolerance));
    HueButton->SetColorAndOpacity(bHue ? FLinearColor::White : Theme::Paused);

    const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
    for (int32 Index = 0; Index < HueChipButtons.Num(); ++Index)
    {
        const bool bSelected = bHue && Angles.IsValidIndex(Index)
            && FJusyncPathlineHUDModel::HueDistance(Hue, Angles[Index]) < 1.0f;
        HueChipButtons[Index]->SetIsEnabled(bOn);
        HueChipButtons[Index]->SetRenderOpacity(!bOn ? 0.35f : (bSelected ? 1.0f : 0.60f));
    }

    ToleranceMinusButton->SetIsEnabled(bOn && bHue);
    TolerancePlusButton->SetIsEnabled(bOn && bHue);
    const float Dim = (bOn && bHue) ? 1.0f : 0.45f;
    ToleranceMinusButton->SetRenderOpacity(Dim);
    TolerancePlusButton->SetRenderOpacity(Dim);
    ToleranceCaption->SetText(FText::FromString(FString::Printf(TEXT("+/-%d"),
        FMath::RoundToInt(Tolerance))));

    // Never clobber a value mid-typing; only mirror the live cvar state.
    if (MaxInput && !MaxInput->HasKeyboardFocus())
    {
        MaxInput->SetText(FText::AsNumber(EffectiveMaxTracks()));
    }
    if (RadiusInput && !RadiusInput->HasKeyboardFocus())
    {
        RadiusInput->SetText(FText::AsNumber(EffectiveRadius()));
    }

    float PointSize = 2.7f;
    if (const IConsoleVariable* CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Morph.CloudScreenSize")))
    {
        PointSize = CVar->GetFloat();
    }
    if (PointSizeInput && !PointSizeInput->HasKeyboardFocus())
    {
        PointSizeInput->SetText(FText::AsNumber(PointSize));
    }

    if (EmissiveInput && !EmissiveInput->HasKeyboardFocus())
    {
        EmissiveInput->SetText(FText::AsNumber(
            Spawner ? Spawner->PathlineEmissiveStrength : 0.35f));
    }

    int32 Sides = 8;
    if (const IConsoleVariable* CVar =
        IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.TubeSides")))
    {
        Sides = CVar->GetInt();
    }
    if (SidesInput && !SidesInput->HasKeyboardFocus())
    {
        SidesInput->SetText(FText::AsNumber(Sides));
    }

    // ---- POINTS section state.
    const bool bPtsVisible = !Spawner || Spawner->bPointCloudsVisible;
    PointsEnableButton->SetIsEnabled(Spawner != nullptr);
    PointsEnableCaption->SetText(bPtsVisible ? LOCTEXT("PointsOnNow", "POINTS ON")
                                             : LOCTEXT("PointsOffNow", "POINTS OFF"));
    PointsEnableButton->SetColorAndOpacity(bPtsVisible ? FLinearColor::White : Theme::Paused);

    const bool bPointHue = Spawner && Spawner->bPointCloudHueFilterEnabled;
    const float PointHue = Spawner ? Spawner->PointCloudHue : 0.0f;
    const float PointTolerance = Spawner ? Spawner->PointCloudHueTolerance : 30.0f;

    PointHueButton->SetIsEnabled(Spawner != nullptr);
    PointHueCaption->SetText(FJusyncPathlineHUDModel::MakeHueLabel(bPointHue, PointHue, PointTolerance));
    PointHueButton->SetColorAndOpacity(bPointHue ? FLinearColor::White : Theme::Paused);

    for (int32 Index = 0; Index < PointHueChipButtons.Num(); ++Index)
    {
        const bool bSelected = bPointHue && Angles.IsValidIndex(Index)
            && FJusyncPathlineHUDModel::HueDistance(PointHue, Angles[Index]) < 1.0f;
        PointHueChipButtons[Index]->SetIsEnabled(Spawner != nullptr);
        PointHueChipButtons[Index]->SetRenderOpacity(
            Spawner ? (bSelected ? 1.0f : 0.60f) : 0.35f);
    }

    PointToleranceMinusButton->SetIsEnabled(bPointHue);
    PointTolerancePlusButton->SetIsEnabled(bPointHue);
    const float PointDim = bPointHue ? 1.0f : 0.45f;
    PointToleranceMinusButton->SetRenderOpacity(PointDim);
    PointTolerancePlusButton->SetRenderOpacity(PointDim);
    PointToleranceCaption->SetText(FText::FromString(FString::Printf(TEXT("+/-%d"),
        FMath::RoundToInt(PointTolerance))));

    if (PointGlowInput && !PointGlowInput->HasKeyboardFocus())
    {
        PointGlowInput->SetText(FText::AsNumber(
            CurrentFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"), 0.004f)));
    }
}

void UJusyncPathlineWidget::HandleEnableClicked()
{
    if (!Spawner)
    {
        return;
    }
    Spawner->bEnablePathlines = !Spawner->bEnablePathlines;
    Refresh();
}

void UJusyncPathlineWidget::HandleColorClicked()
{
    if (!Spawner)
    {
        return;
    }
    Spawner->bPathlineColorByScalar = !Spawner->bPathlineColorByScalar;
    Refresh();
}

void UJusyncPathlineWidget::HandleHueToggle()
{
    if (!Spawner)
    {
        return;
    }
    Spawner->bPathlineHueFilterEnabled = !Spawner->bPathlineHueFilterEnabled;
    Refresh();
}

void UJusyncPathlineWidget::ApplyHueChip(int32 ChipIndex)
{
    if (!Spawner)
    {
        return;
    }
    const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
    if (!Angles.IsValidIndex(ChipIndex))
    {
        return;
    }

    const float Angle = Angles[ChipIndex];
    if (Spawner->bPathlineHueFilterEnabled
        && FJusyncPathlineHUDModel::HueDistance(Spawner->PathlineHue, Angle) < 1.0f)
    {
        // Same chip twice = clear the filter.
        Spawner->bPathlineHueFilterEnabled = false;
    }
    else
    {
        Spawner->PathlineHue = Angle;
        Spawner->bPathlineHueFilterEnabled = true;
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleToleranceMinus()
{
    if (Spawner)
    {
        Spawner->PathlineHueTolerance =
            FJusyncPathlineHUDModel::StepHueTolerance(Spawner->PathlineHueTolerance, +1);
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleTolerancePlus()
{
    if (Spawner)
    {
        Spawner->PathlineHueTolerance =
            FJusyncPathlineHUDModel::StepHueTolerance(Spawner->PathlineHueTolerance, -1);
    }
    Refresh();
}

// ---- POINTS section handlers --------------------------------------------

void UJusyncPathlineWidget::HandlePointsEnableClicked()
{
    if (!Spawner)
    {
        return;
    }
    Spawner->SetPointCloudsVisible(!Spawner->bPointCloudsVisible);
    Refresh();
}

void UJusyncPathlineWidget::HandlePointHueToggle()
{
    if (!Spawner)
    {
        return;
    }
    Spawner->bPointCloudHueFilterEnabled = !Spawner->bPointCloudHueFilterEnabled;
    Spawner->UpdatePointCloudHueFilter();
    Refresh();
}

void UJusyncPathlineWidget::ApplyPointHueChip(int32 ChipIndex)
{
    if (!Spawner)
    {
        return;
    }
    const TArray<float>& Angles = FJusyncPathlineHUDModel::HueChipAngles();
    if (!Angles.IsValidIndex(ChipIndex))
    {
        return;
    }

    const float Angle = Angles[ChipIndex];
    if (Spawner->bPointCloudHueFilterEnabled
        && FJusyncPathlineHUDModel::HueDistance(Spawner->PointCloudHue, Angle) < 1.0f)
    {
        // Same chip twice = clear the filter.
        Spawner->bPointCloudHueFilterEnabled = false;
    }
    else
    {
        Spawner->PointCloudHue = Angle;
        Spawner->bPointCloudHueFilterEnabled = true;
    }
    Spawner->UpdatePointCloudHueFilter();
    Refresh();
}

void UJusyncPathlineWidget::HandlePointToleranceMinus()
{
    if (Spawner)
    {
        // Same convention as the trail filter: minus WIDENS the match.
        Spawner->PointCloudHueTolerance =
            FJusyncPathlineHUDModel::StepHueTolerance(Spawner->PointCloudHueTolerance, +1);
        Spawner->UpdatePointCloudHueFilter();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandlePointTolerancePlus()
{
    if (Spawner)
    {
        Spawner->PointCloudHueTolerance =
            FJusyncPathlineHUDModel::StepHueTolerance(Spawner->PointCloudHueTolerance, -1);
        Spawner->UpdatePointCloudHueFilter();
    }
    Refresh();
}

void UJusyncPathlineWidget::StepPointGlowBy(int32 Direction)
{
    SetFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"),
        FJusyncPathlineHUDModel::StepPointGlow(
            CurrentFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"), 0.004f), Direction));
    Refresh();
}

void UJusyncPathlineWidget::HandlePointGlowInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Text.IsNumeric())
    {
        SetFloatCvar(TEXT("jusync.Spawner.CloudEmissiveFloor"),
            FJusyncPathlineHUDModel::ClampPointGlow(FCString::Atof(*Text.ToString())));
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandlePointSizeInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Text.IsNumeric())
    {
        SetFloatCvar(TEXT("jusync.Morph.CloudScreenSize"),
            FJusyncPathlineHUDModel::ClampPointSize(FCString::Atof(*Text.ToString())));
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleEmissiveInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Spawner && Text.IsNumeric())
    {
        Spawner->PathlineEmissiveStrength =
            FJusyncPathlineHUDModel::ClampEmissive(FCString::Atof(*Text.ToString()));
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleSidesInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Text.IsNumeric())
    {
        if (IConsoleVariable* CVar =
            IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.TubeSides")))
        {
            CVar->Set(FJusyncPathlineHUDModel::ClampTubeSides(FCString::Atoi(*Text.ToString())));
        }
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleMaxInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Text.IsNumeric())
    {
        const int32 Value = FCString::Atoi(*Text.ToString());
        if (IConsoleVariable* CVar =
            IConsoleManager::Get().FindConsoleVariable(TEXT("jusync.Pathlines.MaxTracks")))
        {
            CVar->Set(FJusyncPathlineHUDModel::ClampMaxTracks(Value));
        }
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

void UJusyncPathlineWidget::HandleRadiusInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (Text.IsNumeric())
    {
        const float Value = FCString::Atof(*Text.ToString());
        SetFloatCvar(TEXT("jusync.Pathlines.Radius"), FJusyncPathlineHUDModel::ClampRadius(Value));
    }
    if (CommitType == ETextCommit::OnEnter)
    {
        ReleaseInputFocus();
    }
    Refresh();
}

#undef LOCTEXT_NAMESPACE
