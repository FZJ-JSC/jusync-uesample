#include "JusyncHUDFlipbook.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    struct FClipInfo
    {
        const TCHAR* TexturePath;
        int32 Frames;
        int32 Columns;
        int32 Rows;
        float Fps;
        bool bLoop;
        // Hover settles on its last frame so the chip stays lit while the
        // pointer rests; the other one-shots disappear.
        bool bHoldLast;
    };

    // Must match GRID in Tools/lottie/pack.py.
    const FClipInfo& ClipInfo(JusyncHUDFlipbook::EClip Clip)
    {
        static const FClipInfo Table[] = {
            { TEXT("/Game/JusyncHUD/Animations/T_Hover.T_Hover"), 16, 4, 4, 60.0f, false, true },
            { TEXT("/Game/JusyncHUD/Animations/T_Press.T_Press"), 10, 5, 2, 60.0f, false, false },
            { TEXT("/Game/JusyncHUD/Animations/T_Glow.T_Glow"), 48, 8, 6, 60.0f, true, false },
            { TEXT("/Game/JusyncHUD/Animations/T_Ripple.T_Ripple"), 20, 5, 4, 60.0f, false, false },
        };
        return Table[static_cast<int32>(Clip)];
    }

    struct FAtlas
    {
        TObjectPtr<UTexture2D> Texture = nullptr;
        TArray<FSlateBrush> Frames;
    };

    struct FPlayback
    {
        JusyncHUDFlipbook::EClip Clip = JusyncHUDFlipbook::EClip::Hover;
        TWeakObjectPtr<UImage> Image;
        float Frame = 0.0f;
        float Speed = 1.0f;
        int32 BrushIndex = INDEX_NONE;
        float BaseAlpha = 1.0f;
        float FadeLeft = 0.0f;
        float FadeTotal = 0.0f;
        FLinearColor Tint = FLinearColor::White;
    };

    struct FButtonMotion
    {
        TWeakObjectPtr<UButton> Button;
        TWeakObjectPtr<UImage> Hover;
        TWeakObjectPtr<UImage> Press;
        bool bWasHovered = false;
        bool bWasPressed = false;
        float Scale = 1.0f;
    };

    TMap<uint8, FAtlas> GAtlases;
    // The atlas map is not reflected, so nothing would keep the textures alive
    // across a GC pass without this root.
    TArray<TStrongObjectPtr<UTexture2D>> GAtlasTextures;
    TArray<FPlayback> GPlaybacks;
    TArray<FButtonMotion> GButtons;
    double GLastTickSeconds = 0.0;

    const FAtlas* GetAtlas(JusyncHUDFlipbook::EClip Clip)
    {
        const uint8 Key = static_cast<uint8>(Clip);
        if (const FAtlas* Cached = GAtlases.Find(Key))
        {
            return Cached;
        }

        const FClipInfo& Info = ClipInfo(Clip);
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Info.TexturePath);
        if (!Texture)
        {
            UE_LOG(LogTemp, Warning, TEXT("JUSYNC HUD: missing flipbook atlas %s"), Info.TexturePath);
            return nullptr;
        }

        FAtlas Atlas;
        Atlas.Texture = Texture;
        GAtlasTextures.Emplace(Texture);
        const double Width = Texture->GetSizeX();
        const double Height = Texture->GetSizeY();
        const double CellWidth = Width / Info.Columns;
        const double CellHeight = Height / Info.Rows;
        Atlas.Frames.Reserve(Info.Frames);

        for (int32 Index = 0; Index < Info.Frames; ++Index)
        {
            const int32 Column = Index % Info.Columns;
            const int32 Row = Index / Info.Columns;

            FSlateBrush Brush;
            Brush.DrawAs = ESlateBrushDrawType::Image;
            Brush.Tiling = ESlateBrushTileType::NoTile;
            Brush.SetResourceObject(Texture);
            // Half-texel inset: bilinear filtering at a cell boundary would
            // otherwise blend in the neighbouring frame.
            Brush.SetUVRegion(FBox2d(
                FVector2d((Column * CellWidth + 0.5) / Width, (Row * CellHeight + 0.5) / Height),
                FVector2d(((Column + 1) * CellWidth - 0.5) / Width,
                    ((Row + 1) * CellHeight - 0.5) / Height)));
            Atlas.Frames.Add(MoveTemp(Brush));
        }

        return &GAtlases.Add(Key, MoveTemp(Atlas));
    }

    FPlayback* FindPlayback(const UImage* Image)
    {
        return GPlaybacks.FindByPredicate(
            [Image](const FPlayback& Entry) { return Entry.Image.Get() == Image; });
    }

    void ApplyFrame(FPlayback& Entry)
    {
        UImage* Image = Entry.Image.Get();
        const FAtlas* Atlas = GetAtlas(Entry.Clip);
        if (!Image || !Atlas || Atlas->Frames.Num() == 0)
        {
            return;
        }

        const int32 Index = FMath::Clamp(FMath::FloorToInt(Entry.Frame), 0, Atlas->Frames.Num() - 1);
        if (Index != Entry.BrushIndex)
        {
            Entry.BrushIndex = Index;
            Image->SetBrush(Atlas->Frames[Index]);
        }

        float Alpha = Entry.BaseAlpha;
        if (Entry.FadeTotal > 0.0f)
        {
            Alpha *= FMath::Clamp(Entry.FadeLeft / Entry.FadeTotal, 0.0f, 1.0f);
        }
        if (!FMath::IsNearlyEqual(Alpha, Image->GetColorAndOpacity().A, 0.004f))
        {
            FLinearColor Color = Entry.Tint;
            Color.A = Alpha;
            Image->SetColorAndOpacity(Color);
        }
    }

    void AdvancePlaybacks(float DeltaSeconds)
    {
        for (int32 Index = GPlaybacks.Num() - 1; Index >= 0; --Index)
        {
            FPlayback& Entry = GPlaybacks[Index];
            UImage* Image = Entry.Image.Get();
            if (!Image)
            {
                GPlaybacks.RemoveAtSwap(Index, EAllowShrinking::No);
                continue;
            }

            if (Entry.FadeTotal > 0.0f)
            {
                Entry.FadeLeft -= DeltaSeconds;
                if (Entry.FadeLeft <= 0.0f)
                {
                    Image->SetVisibility(ESlateVisibility::Collapsed);
                    GPlaybacks.RemoveAtSwap(Index, EAllowShrinking::No);
                    continue;
                }
            }

            if (Entry.Speed != 0.0f)
            {
                const FClipInfo& Info = ClipInfo(Entry.Clip);
                Entry.Frame += DeltaSeconds * Info.Fps * Entry.Speed;

                if (Info.bLoop)
                {
                    Entry.Frame = FMath::Fmod(Entry.Frame, static_cast<float>(Info.Frames));
                    if (Entry.Frame < 0.0f)
                    {
                        Entry.Frame += static_cast<float>(Info.Frames);
                    }
                }
                else if (Entry.Frame >= static_cast<float>(Info.Frames) - 1.0f)
                {
                    Entry.Frame = static_cast<float>(Info.Frames) - 1.0f;
                    ApplyFrame(Entry);
                    if (Info.bHoldLast)
                    {
                        GPlaybacks.RemoveAtSwap(Index, EAllowShrinking::No);
                    }
                    else
                    {
                        Image->SetVisibility(ESlateVisibility::Collapsed);
                        GPlaybacks.RemoveAtSwap(Index, EAllowShrinking::No);
                    }
                    continue;
                }
            }

            ApplyFrame(Entry);
        }
    }

    void SampleButtons(float DeltaSeconds)
    {
        for (int32 Index = GButtons.Num() - 1; Index >= 0; --Index)
        {
            FButtonMotion& Motion = GButtons[Index];
            UButton* Button = Motion.Button.Get();
            if (!Button)
            {
                GButtons.RemoveAtSwap(Index, EAllowShrinking::No);
                continue;
            }

            const bool bHovered = Button->IsHovered();
            if (bHovered != Motion.bWasHovered)
            {
                Motion.bWasHovered = bHovered;
                if (UImage* Hover = Motion.Hover.Get())
                {
                    if (bHovered)
                    {
                        JusyncHUDFlipbook::Play(Hover, JusyncHUDFlipbook::EClip::Hover, 1.0f);
                    }
                    else
                    {
                        JusyncHUDFlipbook::FadeOut(Hover);
                    }
                }
            }

            const bool bPressed = Button->IsPressed();
            if (bPressed && !Motion.bWasPressed)
            {
                if (UImage* Press = Motion.Press.Get())
                {
                    JusyncHUDFlipbook::Play(Press, JusyncHUDFlipbook::EClip::Press, 1.15f);
                }
            }
            Motion.bWasPressed = bPressed;

            // The chip itself answers the pointer: the halo alone reads as a
            // backdrop, the small grow/shrink is what makes the press feel physical.
            const float TargetScale = bPressed ? 0.96f : (bHovered ? 1.05f : 1.0f);
            const float Previous = Motion.Scale;
            Motion.Scale = FMath::FInterpTo(Motion.Scale, TargetScale, DeltaSeconds, 24.0f);
            if (!FMath::IsNearlyEqual(Previous, Motion.Scale, 0.0005f))
            {
                Button->SetRenderScale(FVector2D(Motion.Scale, Motion.Scale));
            }
        }
    }
}

UCanvasPanel* JusyncHUDFlipbook::WrapButton(UWidgetTree* Tree, UButton* Button, bool bHover,
    float SpillPx)
{
    if (!Tree || !Button)
    {
        return nullptr;
    }

    UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), NAME_None);
    Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UCanvasPanelSlot* ButtonSlot = Canvas->AddChildToCanvas(Button);
    ButtonSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    ButtonSlot->SetOffsets(FMargin(0.0f));
    ButtonSlot->SetZOrder(1);
    Button->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

    auto AddOverlay = [Tree, Canvas, SpillPx](EClip Clip, const FLinearColor& Tint, int32 ZOrder)
    {
        UImage* Image = MakeImage(Tree, Clip, Tint);
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Image);
        Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        // Growing the slot instead of render-scaling the image keeps the halo on
        // the plain quad path, which is what carries a brush UV region.
        Slot->SetOffsets(FMargin(-SpillPx));
        Slot->SetZOrder(ZOrder);
        return Image;
    };

    // The halo sits under the chip and only spills past its edges; the press
    // flash is drawn over the chip, where the eye already is when the click lands.
    UImage* HoverImage = bHover
        ? AddOverlay(EClip::Hover, FLinearColor(1.0f, 1.0f, 1.0f, 0.95f), 0)
        : nullptr;
    UImage* PressImage = AddOverlay(EClip::Press, FLinearColor(1.0f, 1.0f, 1.0f, 0.9f), 2);

    FButtonMotion Motion;
    Motion.Button = Button;
    Motion.Hover = HoverImage;
    Motion.Press = PressImage;
    GButtons.Add(MoveTemp(Motion));

    return Canvas;
}

UImage* JusyncHUDFlipbook::MakeImage(UWidgetTree* Tree, EClip Clip, const FLinearColor& Tint)
{
    UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), NAME_None);
    Image->SetVisibility(ESlateVisibility::Collapsed);
    Image->SetColorAndOpacity(Tint);

    if (const FAtlas* Atlas = GetAtlas(Clip))
    {
        if (Atlas->Frames.Num() > 0)
        {
            Image->SetBrush(Atlas->Frames[0]);
        }
    }
    return Image;
}

void JusyncHUDFlipbook::Play(UImage* Image, EClip Clip, float Speed)
{
    if (!Image)
    {
        return;
    }

    Image->SetVisibility(ESlateVisibility::HitTestInvisible);

    FPlayback* Existing = FindPlayback(Image);
    if (!Existing)
    {
        FPlayback Entry;
        Entry.Image = Image;
        Entry.Tint = Image->GetColorAndOpacity();
        Entry.Tint.A = 1.0f;
        GPlaybacks.Add(MoveTemp(Entry));
        Existing = &GPlaybacks.Last();
    }

    Existing->Clip = Clip;
    Existing->Frame = Speed < 0.0f ? static_cast<float>(ClipInfo(Clip).Frames - 1) : 0.0f;
    Existing->Speed = Speed;
    Existing->BrushIndex = INDEX_NONE;
    Existing->FadeTotal = 0.0f;
    Existing->FadeLeft = 0.0f;
    ApplyFrame(*Existing);
}

void JusyncHUDFlipbook::FadeOut(UImage* Image)
{
    if (!Image)
    {
        return;
    }

    FPlayback* Existing = FindPlayback(Image);
    if (!Existing)
    {
        // The hover clip already settled on its last frame and left the table;
        // fade that frame out without restarting the animation.
        FPlayback Entry;
        Entry.Image = Image;
        Entry.Clip = EClip::Hover;
        Entry.Tint = Image->GetColorAndOpacity();
        Entry.Tint.A = 1.0f;
        Entry.Frame = static_cast<float>(ClipInfo(EClip::Hover).Frames - 1);
        Entry.Speed = 0.0f;
        Entry.BrushIndex = INDEX_NONE;
        GPlaybacks.Add(MoveTemp(Entry));
        Existing = &GPlaybacks.Last();
    }

    Existing->Speed = 0.0f;
    Existing->FadeTotal = 0.16f;
    Existing->FadeLeft = 0.16f;
}

void JusyncHUDFlipbook::Stop(UImage* Image)
{
    if (!Image)
    {
        return;
    }
    Image->SetVisibility(ESlateVisibility::Collapsed);
    GPlaybacks.RemoveAll([Image](const FPlayback& Entry) { return Entry.Image.Get() == Image; });
}

void JusyncHUDFlipbook::Tick()
{
    const double Now = FPlatformTime::Seconds();
    const float DeltaSeconds = static_cast<float>(Now - GLastTickSeconds);
    // Several widgets tick per frame; only the first call advances time.
    if (DeltaSeconds < 0.0005f || DeltaSeconds > 0.5f)
    {
        GLastTickSeconds = Now;
        return;
    }
    GLastTickSeconds = Now;

    SampleButtons(DeltaSeconds);
    AdvancePlaybacks(DeltaSeconds);
}
