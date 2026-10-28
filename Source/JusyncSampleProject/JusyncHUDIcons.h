#pragma once

#include "CoreMinimal.h"

class UImage;
class UTexture2D;
class UWidgetTree;

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

    UImage* MakeImage(UWidgetTree* Tree, EIcon Icon, float Size,
        const FLinearColor& Color = FLinearColor::White);

    void SetImageIcon(UImage* Image, EIcon Icon);

    UTexture2D* GetTexture(EIcon Icon);
}
