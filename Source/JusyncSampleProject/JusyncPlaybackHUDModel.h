#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

class FJusyncPlaybackHUDModel
{
public:
    static bool CanScrub(int32 SampleCount);
    static int32 ScrubPositionToIndex(float Position, int32 SampleCount);
    static float SampleIndexToScrubPosition(int32 Index, int32 SampleCount);
    static FText MakeStatusText(int32 Index, int32 SampleCount, double SampleTime, bool bIsLive);
    static FString MakeTimestepLabel(int32 Index, int32 SampleCount, double SampleTime);
    static int32 ComboSelectionToSampleIndex(int32 SelectionIndex);
    static int32 SampleIndexToComboSelection(int32 Index, int32 SampleCount, bool bIsLive);

    static FText MakePlayPauseLabel(bool bParkedInHistory);

    static FText MakePathlineLabel(bool bPathlinesEnabled);

    static float LiveEdgeFreezePosition();
};
