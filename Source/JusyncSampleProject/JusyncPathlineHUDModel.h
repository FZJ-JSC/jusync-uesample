#pragma once

#include "CoreMinimal.h"

/**
 * Pure maths and label formats for the pathline control panel. Lives apart
 * from UJusyncPathlineWidget so the stepping rules stay unit-testable, in the
 * same spirit as FJusyncPlaybackHUDModel for the transport panel.
 */
class FJusyncPathlineHUDModel
{
public:
    /** Geometric ladder for the MAX TRACKS stepper (budget vs legibility). */
    static const TArray<int32>& TrackPresets();

    /**
     * Moves one preset up (Direction > 0) or down (Direction < 0). A current
     * value that is not a preset snaps to the next one in the pressed
     * direction; the ends are clamped.
     */
    static int32 StepTrackPreset(int32 Current, int32 Direction);

    /** Moves a scalar-filter bound by 5% of the normalized range, clamped. */
    static float StepFilterBound(float Current, int32 Direction);

    /** Wheel distance between two hues (degrees, wrapping through red). */
    static float HueDistance(float HueA, float HueB);

    /** True when Hue lies within Tolerance degrees of the selected hue. */
    static bool AcceptsHue(float Hue, float Selected, float Tolerance);

    /** Clamp a tolerance step (degrees) to a sane picker range. */
    static float StepHueTolerance(float Current, int32 Direction);

    /** Hard limits for typed panel inputs (shared with the widget handlers). */
    static float ClampRadius(float ValueCm);
    static int32 ClampMaxTracks(int32 Value);
    static float ClampPointSize(float Value);
    static float ClampEmissive(float Value);
    static int32 ClampTubeSides(int32 Value);

    /**
     * Linear ± stepper moves for the typed inputs, each result passed through
     * its Clamp* above. Radius/point-size/emission move by 0.05 per click,
     * tube sides by whole sides.
     */
    static float StepRadius(float Current, int32 Direction);
    static float StepPointSize(float Current, int32 Direction);
    static float StepEmissive(float Current, int32 Direction);
    static int32 StepTubeSides(int32 Current, int32 Direction);

    /** POINTS section: cloud emissive floor (self-glow), fine 0.005 steps. */
    static float ClampPointGlow(float Value);
    static float StepPointGlow(float Current, int32 Direction);

    /** Chip colours for the picker row (red, yellow, green, cyan, blue, magenta). */
    static const TArray<FLinearColor>& HueChipColors();
    static const TArray<float>& HueChipAngles();

    static FText MakeStatusLabel(int32 ActiveTracks, int32 VisibleTracks, int64 Nodes);
    static FText MakeTrackLabel(int32 MaxTracks);
    static FText MakeFilterLabel(bool bFilterEnabled, float FilterMin, float FilterMax);
    static FText MakeHueLabel(bool bHueEnabled, float HueDeg, float Tolerance);
    static FText MakeColorLabel(bool bColorByScalar);
    static FText MakeRadiusLabel(float RadiusCm);
};
