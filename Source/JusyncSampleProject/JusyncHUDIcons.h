#pragma once

#include "CoreMinimal.h"

class UImage;
class UTexture2D;
class UWidgetTree;

/**
 * Small, monochrome HUD icon set generated at runtime.
 *
 * The deck is built in C++ with no Widget Blueprint or icon-font dependency,
 * so the transport and stepper glyphs are rasterised once into transient
 * textures. They are white with an alpha mask; UImage tints them to the
 * current text colour.
 */
namespace JusyncHUDIcons
{
    enum class EIcon : uint8
    {
        Play,
        Pause,
        Previous,
        Next,
        Live,
        Plus,
        Minus,
        Square,
        Circle,
        ChevronDown,
        Loop
    };

    /** Creates a tinted image at the requested Slate size. */
    UImage* MakeImage(UWidgetTree* Tree, EIcon Icon, float Size,
        const FLinearColor& Color = FLinearColor::White);

    /** Swaps the glyph on an image created by MakeImage. */
    void SetImageIcon(UImage* Image, EIcon Icon);

    /**
     * Returns the cached white-alpha texture for a glyph. Slate brushes that
     * are part of a widget style (for example the combo-box down arrow) can
     * use this instead of the editor's default icon brush, which is not
     * guaranteed to be present in a packaged game.
     */
    UTexture2D* GetTexture(EIcon Icon);
}
