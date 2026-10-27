#include "JusyncPathlineHUDModel.h"

#define LOCTEXT_NAMESPACE "JusyncPathlineHUDModel"

namespace
{
    constexpr float FilterStep = 0.05f;

    FText AsFixed2(float Value)
    {
        return FText::FromString(FString::Printf(TEXT("%.2f"), Value));
    }
}

const TArray<int32>& FJusyncPathlineHUDModel::TrackPresets()
{
    static const TArray<int32> Presets = { 25, 50, 100, 200, 300, 600, 1000 };
    return Presets;
}

int32 FJusyncPathlineHUDModel::StepTrackPreset(int32 Current, int32 Direction)
{
    const TArray<int32>& Presets = TrackPresets();
    if (Direction == 0 || Presets.Num() == 0)
    {
        return Current;
    }

    if (Direction > 0)
    {
        for (const int32 Preset : Presets)
        {
            if (Preset > Current)
            {
                return Preset;
            }
        }
        return Presets.Last();
    }

    for (int32 Index = Presets.Num() - 1; Index >= 0; --Index)
    {
        if (Presets[Index] < Current)
        {
            return Presets[Index];
        }
    }
    return Presets[0];
}

float FJusyncPathlineHUDModel::StepFilterBound(float Current, int32 Direction)
{
    return FMath::Clamp(Current + FilterStep * FMath::Sign(static_cast<float>(Direction)), 0.0f, 1.0f);
}

float FJusyncPathlineHUDModel::HueDistance(float HueA, float HueB)
{
    const float Delta = FMath::Abs(FMath::Fmod(HueA - HueB + 360.0f, 360.0f));
    return FMath::Min(Delta, 360.0f - Delta);
}

bool FJusyncPathlineHUDModel::AcceptsHue(float Hue, float Selected, float Tolerance)
{
    return HueDistance(Hue, Selected) <= Tolerance;
}

float FJusyncPathlineHUDModel::StepHueTolerance(float Current, int32 Direction)
{
    return FMath::Clamp(Current + 10.0f * FMath::Sign(static_cast<float>(Direction)), 5.0f, 180.0f);
}

float FJusyncPathlineHUDModel::ClampRadius(float ValueCm)
{
    return FMath::Clamp(ValueCm, 0.02f, 50.0f);
}

int32 FJusyncPathlineHUDModel::ClampMaxTracks(int32 Value)
{
    return FMath::Clamp(Value, 1, 1000);
}

float FJusyncPathlineHUDModel::ClampPointSize(float Value)
{
    return FMath::Clamp(Value, 0.01f, 10.0f);
}

float FJusyncPathlineHUDModel::ClampEmissive(float Value)
{
    return FMath::Clamp(Value, 0.0f, 4.0f);
}

int32 FJusyncPathlineHUDModel::ClampTubeSides(int32 Value)
{
    return FMath::Clamp(Value, 3, 16);
}

float FJusyncPathlineHUDModel::StepRadius(float Current, int32 Direction)
{
    return ClampRadius(Current + 0.05f * FMath::Sign(static_cast<float>(Direction)));
}

float FJusyncPathlineHUDModel::StepPointSize(float Current, int32 Direction)
{
    return ClampPointSize(Current + 0.05f * FMath::Sign(static_cast<float>(Direction)));
}

float FJusyncPathlineHUDModel::StepEmissive(float Current, int32 Direction)
{
    return ClampEmissive(Current + 0.05f * FMath::Sign(static_cast<float>(Direction)));
}

float FJusyncPathlineHUDModel::ClampPointGlow(float Value)
{
    return FMath::Clamp(Value, 0.0f, 1.0f);
}

float FJusyncPathlineHUDModel::StepPointGlow(float Current, int32 Direction)
{
    // Cloud emissive floor lives near 0.004 - coarse 0.05 steps would snap
    // it off/on, so the POINTS stepper uses fine increments.
    return ClampPointGlow(Current + 0.005f * FMath::Sign(static_cast<float>(Direction)));
}

int32 FJusyncPathlineHUDModel::StepTubeSides(int32 Current, int32 Direction)
{
    return ClampTubeSides(Current + FMath::Sign(Direction));
}

const TArray<FLinearColor>& FJusyncPathlineHUDModel::HueChipColors()
{
    static const TArray<FLinearColor> Colors = {
        FLinearColor::Red,
        FLinearColor(1.0f, 1.0f, 0.0f),
        FLinearColor(0.0f, 1.0f, 0.0f),
        FLinearColor(0.0f, 1.0f, 1.0f),
        FLinearColor(0.0f, 0.0f, 1.0f),
        FLinearColor(1.0f, 0.0f, 1.0f)
    };
    return Colors;
}

const TArray<float>& FJusyncPathlineHUDModel::HueChipAngles()
{
    static const TArray<float> Angles = { 0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f };
    return Angles;
}

FText FJusyncPathlineHUDModel::MakeStatusLabel(int32 ActiveTracks, int32 VisibleTracks, int64 Nodes)
{
    return FText::Format(
        LOCTEXT("PathlineStatus", "active {0}  ·  shown {1}  ·  nodes {2}"),
        FText::AsNumber(ActiveTracks), FText::AsNumber(VisibleTracks), FText::AsNumber(Nodes));
}

FText FJusyncPathlineHUDModel::MakeTrackLabel(int32 MaxTracks)
{
    return FText::Format(LOCTEXT("PathlineTracks", "MAX {0}"), FText::AsNumber(MaxTracks));
}

FText FJusyncPathlineHUDModel::MakeFilterLabel(bool bFilterEnabled, float FilterMin, float FilterMax)
{
    if (!bFilterEnabled)
    {
        return LOCTEXT("PathlineFilterOff", "FILTER OFF");
    }
    return FText::Format(LOCTEXT("PathlineFilterOn", "FILTER {0}-{1}"),
        AsFixed2(FilterMin), AsFixed2(FilterMax));
}

FText FJusyncPathlineHUDModel::MakeHueLabel(bool bHueEnabled, float HueDeg, float Tolerance)
{
    if (!bHueEnabled)
    {
        return LOCTEXT("PathlineHueOff", "HUE OFF");
    }
    return FText::Format(LOCTEXT("PathlineHueOn", "HUE {0} +/-{1}"),
        FText::AsNumber(FMath::RoundToInt(HueDeg)), FText::AsNumber(FMath::RoundToInt(Tolerance)));
}

FText FJusyncPathlineHUDModel::MakeColorLabel(bool bColorByScalar)
{
    return bColorByScalar
        ? LOCTEXT("PathlineColorScalar", "COLOR SCALAR")
        : LOCTEXT("PathlineColorFlat", "COLOR FLAT");
}

FText FJusyncPathlineHUDModel::MakeRadiusLabel(float RadiusCm)
{
    return FText::Format(LOCTEXT("PathlineRadius", "R {0}"), AsFixed2(RadiusCm));
}

#undef LOCTEXT_NAMESPACE
