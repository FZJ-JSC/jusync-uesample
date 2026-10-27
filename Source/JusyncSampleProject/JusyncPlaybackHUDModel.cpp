#include "JusyncPlaybackHUDModel.h"

bool FJusyncPlaybackHUDModel::CanScrub(int32 SampleCount)
{
    return SampleCount > 1;
}

int32 FJusyncPlaybackHUDModel::ScrubPositionToIndex(float Position, int32 SampleCount)
{
    if (SampleCount <= 1)
    {
        return 0;
    }

    const float Last = static_cast<float>(SampleCount - 1);
    return FMath::Clamp(FMath::RoundToInt(FMath::Clamp(Position, 0.0f, 1.0f) * Last), 0, SampleCount - 1);
}

float FJusyncPlaybackHUDModel::SampleIndexToScrubPosition(int32 Index, int32 SampleCount)
{
    if (SampleCount <= 1)
    {
        return 0.0f;
    }

    return static_cast<float>(Index) / static_cast<float>(SampleCount - 1);
}

FText FJusyncPlaybackHUDModel::MakeStatusText(int32 Index, int32 SampleCount, double SampleTime, bool bIsLive)
{
    if (SampleCount <= 0)
    {
        return FText::FromString(TEXT("no samples yet"));
    }

    const int32 Clamped = FMath::Clamp(Index, 0, SampleCount - 1);
    const FString Position = FString::Printf(TEXT("[%d / %d]"), Clamped + 1, SampleCount);

    if (bIsLive)
    {
        // Live playback shows the sender's timecode too, so the counter can
        // be read against ParaView's timeline without pausing first.
        if (SampleTime >= 0.0)
        {
            return FText::FromString(FString::Printf(TEXT("LIVE   t=%.3g   %s"), SampleTime, *Position));
        }
        return FText::FromString(FString::Printf(TEXT("LIVE   %s"), *Position));
    }

    if (SampleTime >= 0.0)
    {
        return FText::FromString(FString::Printf(TEXT("t=%.3g   %s"), SampleTime, *Position));
    }

    return FText::FromString(Position);
}

FString FJusyncPlaybackHUDModel::MakeTimestepLabel(int32 Index, int32 SampleCount, double SampleTime)
{
    const FString Row = FString::Printf(TEXT("#%d / %d"), FMath::Clamp(Index, 0, FMath::Max(0, SampleCount - 1)) + 1, SampleCount);
    return SampleTime >= 0.0 ? FString::Printf(TEXT("%s   t=%.3g"), *Row, SampleTime) : Row;
}

int32 FJusyncPlaybackHUDModel::ComboSelectionToSampleIndex(int32 SelectionIndex)
{
    // Row 0 is the LIVE entry.
    return SelectionIndex <= 0 ? -1 : SelectionIndex - 1;
}

int32 FJusyncPlaybackHUDModel::SampleIndexToComboSelection(int32 Index, int32 SampleCount, bool bIsLive)
{
    if (bIsLive)
    {
        return 0;
    }

    return FMath::Clamp(Index, 0, FMath::Max(0, SampleCount - 1)) + 1;
}

FText FJusyncPlaybackHUDModel::MakePlayPauseLabel(bool bParkedInHistory)
{
    return FText::FromString(bParkedInHistory ? TEXT("PLAY") : TEXT("PAUSE"));
}

FText FJusyncPlaybackHUDModel::MakePathlineLabel(bool bPathlinesEnabled)
{
    return FText::FromString(bPathlinesEnabled ? TEXT("PATHLINES ON") : TEXT("PATHLINES OFF"));
}

float FJusyncPlaybackHUDModel::LiveEdgeFreezePosition()
{
    return 0.998f;
}
