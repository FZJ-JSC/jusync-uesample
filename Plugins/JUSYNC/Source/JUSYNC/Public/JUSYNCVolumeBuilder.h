#pragma once

#include "CoreMinimal.h"
#include "LidarPointCloudShared.h"

struct JUSYNC_API FJUSYNCVolumeGrid
{
    int32 Resolution = 0;
    FBox Bounds = FBox(ForceInit);
    // Packed BGRA8, uploaded straight into the volume render target: rgb is the
    // payload's own point colour, a is the normalized density that drives extinction.
    TArray<uint8> Voxels;
    int64 NumPoints = 0;
    float PeakDensity = 0.f;

    bool IsValid() const
    {
        return Resolution > 0
            && Voxels.Num() == Resolution * Resolution * Resolution * 4
            && Bounds.IsValid;
    }

    int32 Index(int32 X, int32 Y, int32 Z) const
    {
        return (Z * Resolution + Y) * Resolution + X;
    }
};

using FJUSYNCPointArray = TArray64<FLidarPointCloudPoint>;
using FJUSYNCPointArrayPtr = TSharedPtr<FJUSYNCPointArray, ESPMode::ThreadSafe>;
using FJUSYNCVolumeGridPtr = TSharedPtr<FJUSYNCVolumeGrid, ESPMode::ThreadSafe>;

class JUSYNC_API FJUSYNCVolumeBuilder
{
public:
    // Positions are transformed by WorldScale/WorldOffset before scatter, so the
    // grid and its bounds live in the same space the material samples in.
    // SplatRadius <= 0 collapses every point onto its own voxel.
    static bool Build(const TArray64<FLidarPointCloudPoint>& Points,
                      const FVector& WorldScale, const FVector& WorldOffset,
                      int32 Resolution, int32 BlurPasses, float SplatRadius,
                      FJUSYNCVolumeGrid& Out);
};
