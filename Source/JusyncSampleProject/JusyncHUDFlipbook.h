#pragma once

#include "CoreMinimal.h"

class UButton;
class UCanvasPanel;
class UImage;
class UWidgetTree;

// Flipbook playback for the HUD. Clips are baked offline (Tools/lottie) into one
// atlas per animation: a grid of transparent frames, row-major. Playing a clip
// swaps a brush whose UV region picks one cell, so no material, no texture
// upload and no per-frame rasterization is involved.
namespace JusyncHUDFlipbook
{
    enum class EClip : uint8
    {
        Hover,
        Press,
        Glow,
        Ripple,
    };

    // Puts the button inside a canvas with hover and press overlays, and returns
    // that canvas for the caller to place in the layout. SpillPx is how far the
    // halo reaches past the chip on every side.
    UCanvasPanel* WrapButton(UWidgetTree* Tree, UButton* Button, bool bHover = true,
        float SpillPx = 20.0f);

    // Image playing nothing until Play() is called.
    UImage* MakeImage(UWidgetTree* Tree, EClip Clip, const FLinearColor& Tint);

    // Restarts the clip. Speed multiplies the baked rate (negative plays back).
    void Play(UImage* Image, EClip Clip, float Speed = 1.0f);

    // Fades the current image out and hides it; used when a hover ends.
    void FadeOut(UImage* Image);

    void Stop(UImage* Image);

    // Advances every animation and samples button hover/press state. Call it
    // from NativeTick; extra calls in the same frame are ignored.
    void Tick();
}
