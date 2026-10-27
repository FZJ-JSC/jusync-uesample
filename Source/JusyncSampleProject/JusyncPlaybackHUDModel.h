#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

/**
 * Pure helpers behind the in-game JUSYNC playback HUD.
 *
 * Deliberately free of UObjects and Slate so the slider/index maths and the
 * label formatting can be tested without a world or a widget.
 */
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

    /**
     * Caption for the transport toggle: PAUSE while following the stream,
     * PLAY while parked on a cached sample.
     */
    static FText MakePlayPauseLabel(bool bParkedInHistory);

    /** Caption for the trail toggle chip, reflecting the spawner setting. */
    static FText MakePathlineLabel(bool bPathlinesEnabled);

    /**
     * Playback position that freezes the view on the newest cached sample.
     * The spawner treats anything at or above 0.999 as the live edge, and
     * ApplyCachedSample rounds to the nearest index, so 0.998 parks on the
     * last sample while still counting as "playing a cached sample".
     */
    static float LiveEdgeFreezePosition();
};
