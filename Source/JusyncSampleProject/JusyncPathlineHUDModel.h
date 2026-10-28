#pragma once

#include "CoreMinimal.h"

class FJusyncPathlineHUDModel
{
public:
    static const TArray<int32>& TrackPresets();

    static int32 StepTrackPreset(int32 Current, int32 Direction);

    static float StepFilterBound(float Current, int32 Direction);

    static float HueDistance(float HueA, float HueB);

    static bool AcceptsHue(float Hue, float Selected, float Tolerance);

    static float StepHueTolerance(float Current, int32 Direction);

    static float ClampRadius(float ValueCm);
    static int32 ClampMaxTracks(int32 Value);
    static float ClampPointSize(float Value);
    static float ClampEmissive(float Value);
    static int32 ClampTubeSides(int32 Value);

    static float StepRadius(float Current, int32 Direction);
    static float StepPointSize(float Current, int32 Direction);
    static float StepEmissive(float Current, int32 Direction);
    static int32 StepTubeSides(int32 Current, int32 Direction);

    static float ClampPointGlow(float Value);
    static float StepPointGlow(float Current, int32 Direction);

    static const TArray<FLinearColor>& HueChipColors();
    static const TArray<float>& HueChipAngles();

    static FText MakeStatusLabel(int32 ActiveTracks, int32 VisibleTracks, int64 Nodes);
    static FText MakeTrackLabel(int32 MaxTracks);
    static FText MakeFilterLabel(bool bFilterEnabled, float FilterMin, float FilterMax);
    static FText MakeHueLabel(bool bHueEnabled, float HueDeg, float Tolerance);
    static FText MakeColorLabel(bool bColorByScalar);
    static FText MakeRadiusLabel(float RadiusCm);
};
