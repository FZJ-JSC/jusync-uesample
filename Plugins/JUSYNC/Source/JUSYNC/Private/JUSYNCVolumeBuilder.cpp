#include "JUSYNCVolumeBuilder.h"

#include "Async/ParallelFor.h"
#include "HAL/PlatformAtomics.h"

namespace
{
    void BlurX(const TArray<float>& Src, TArray<float>& Dst, int32 Res)
    {
        ParallelFor(Res, [&](int32 Z)
        {
            for (int32 Y = 0; Y < Res; ++Y)
            {
                const int32 Row = (Z * Res + Y) * Res;
                for (int32 X = 0; X < Res; ++X)
                {
                    const int32 XM = X > 0 ? X - 1 : 0;
                    const int32 XP = X + 1 < Res ? X + 1 : Res - 1;
                    Dst[Row + X] = (Src[Row + XM] + 2.f * Src[Row + X] + Src[Row + XP]) * 0.25f;
                }
            }
        });
    }

    void BlurY(const TArray<float>& Src, TArray<float>& Dst, int32 Res)
    {
        ParallelFor(Res, [&](int32 Z)
        {
            for (int32 Y = 0; Y < Res; ++Y)
            {
                const int32 Row = (Z * Res + Y) * Res;
                const int32 RowM = (Z * Res + (Y > 0 ? Y - 1 : 0)) * Res;
                const int32 RowP = (Z * Res + (Y + 1 < Res ? Y + 1 : Res - 1)) * Res;
                for (int32 X = 0; X < Res; ++X)
                {
                    Dst[Row + X] = (Src[RowM + X] + 2.f * Src[Row + X] + Src[RowP + X]) * 0.25f;
                }
            }
        });
    }

    void BlurZ(const TArray<float>& Src, TArray<float>& Dst, int32 Res)
    {
        const int32 Slice = Res * Res;
        ParallelFor(Res, [&](int32 Z)
        {
            const int32 ZM = (Z > 0 ? Z - 1 : 0) * Slice;
            const int32 ZP = (Z + 1 < Res ? Z + 1 : Res - 1) * Slice;
            const int32 Row = Z * Slice;
            for (int32 I = 0; I < Slice; ++I)
            {
                Dst[Row + I] = (Src[ZM + I] + 2.f * Src[Row + I] + Src[ZP + I]) * 0.25f;
            }
        });
    }
}

bool FJUSYNCVolumeBuilder::Build(const TArray64<FLidarPointCloudPoint>& Points,
                                 const FVector& WorldScale, const FVector& WorldOffset,
                                 int32 Resolution, int32 BlurPasses, float SplatRadius,
                                 FJUSYNCVolumeGrid& Out)
{
    Out = FJUSYNCVolumeGrid();

    // A voxel finer than the point spacing yields isolated specks: one point per
    // voxel, no interior, and the march renders only falloff (which reads as glow).
    // Cap the grid near the spacing a surface cloud of this many points can feed.
    const int32 PointCap = FMath::Clamp(
        FMath::RoundToInt(FMath::Sqrt(static_cast<float>(Points.Num())) * 1.25f / 32.f) * 32, 64, 512);
    const int32 Res = FMath::Min(Resolution > 0 ? Resolution : PointCap, PointCap);
    if (Resolution > Res)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[Volume] resolution %d exceeds the density of %d points; using %d"),
            Resolution, Points.Num(), Res);
    }
    if (Res < 8 || Points.Num() == 0)
    {
        return false;
    }

    FBox Bounds(ForceInit);
    for (int64 I = 0; I < Points.Num(); ++I)
    {
        const FLidarPointCloudPoint& P = Points[I];
        if (P.bVisible)
        {
            Bounds += FVector(P.Location) * WorldScale + WorldOffset;
        }
    }

    if (!Bounds.IsValid)
    {
        return false;
    }

    // Tight bounds put the outermost points exactly on the sampling limit, where
    // the material's ray-box test drops them; grow past the data instead.
    const FVector Center = Bounds.GetCenter();
    FVector Half = Bounds.GetExtent();
    Half.X = FMath::Max(Half.X, 1.0);
    Half.Y = FMath::Max(Half.Y, 1.0);
    Half.Z = FMath::Max(Half.Z, 1.0);
    Half *= 1.02;
    Bounds = FBox(Center - Half, Center + Half);

    const FVector Size = Bounds.GetSize();
    const FVector Min = Bounds.Min;

    const int32 Voxels = Res * Res * Res;
    TArray<int32> Counts;
    Counts.SetNumZeroed(Voxels);

    // One point's BGRA per voxel. When several points share a voxel which one wins
    // is not deterministic, and for showing the payload's own colours it does not
    // matter; the density-weighted average below smooths the result.
    TArray<uint32> ColorPacked;
    ColorPacked.Init(0, Voxels);

    const int32 Splat = FMath::Clamp(FMath::CeilToInt(SplatRadius), 0, 4);
    const float Sigma = SplatRadius > 0.f ? FMath::Max(SplatRadius * 0.5f, 0.5f) : 0.5f;
    const float InvSigma2 = 1.f / (2.f * Sigma * Sigma);

    const int32 NumPoints = static_cast<int32>(Points.Num());
    ParallelFor(NumPoints, [&](int32 Index)
    {
        const FLidarPointCloudPoint& P = Points[Index];
        if (!P.bVisible)
        {
            return;
        }

        const FVector W = FVector(P.Location) * WorldScale + WorldOffset;
        const int32 X = FMath::FloorToInt((W.X - Min.X) / Size.X * Res);
        const int32 Y = FMath::FloorToInt((W.Y - Min.Y) / Size.Y * Res);
        const int32 Z = FMath::FloorToInt((W.Z - Min.Z) / Size.Z * Res);

        if (X < 0 || X >= Res || Y < 0 || Y >= Res || Z < 0 || Z >= Res)
        {
            return;
        }

        // Bytes ordered B,G,R,A to match PF_B8G8R8A8. Kept as raw bytes rather than
        // sRGB-decoded so the volume matches the point cloud, whose vertex colours
        // reach the shader the same way.
        const FColor& Raw = P.Color;
        const uint32 Packed = (uint32(Raw.A) << 24) | (uint32(Raw.R) << 16)
            | (uint32(Raw.G) << 8) | uint32(Raw.B);

        if (Splat == 0)
        {
            ColorPacked[(Z * Res + Y) * Res + X] = Packed;
            FPlatformAtomics::InterlockedIncrement(reinterpret_cast<volatile int32*>(&Counts[(Z * Res + Y) * Res + X]));
            return;
        }

        for (int32 DZ = -Splat; DZ <= Splat; ++DZ)
        {
            const int32 ZZ = Z + DZ;
            if (ZZ < 0 || ZZ >= Res) continue;
            for (int32 DY = -Splat; DY <= Splat; ++DY)
            {
                const int32 YY = Y + DY;
                if (YY < 0 || YY >= Res) continue;
                for (int32 DX = -Splat; DX <= Splat; ++DX)
                {
                    const int32 XX = X + DX;
                    if (XX < 0 || XX >= Res) continue;

                    const float Weight = FMath::Exp(-(DX * DX + DY * DY + DZ * DZ) * InvSigma2);
                    const int32 Quantized = FMath::Max(1, FMath::RoundToInt(Weight * 64.f));
                    ColorPacked[(ZZ * Res + YY) * Res + XX] = Packed;
                    FPlatformAtomics::InterlockedAdd(reinterpret_cast<volatile int32*>(&Counts[(ZZ * Res + YY) * Res + XX]), Quantized);
                }
            }
        }
    });

    TArray<float> Field;
    Field.SetNumUninitialized(Voxels);
    for (int32 I = 0; I < Voxels; ++I)
    {
        Field[I] = static_cast<float>(Counts[I]);
    }
    Counts.Empty();

    TArray<float> Scratch;
    Scratch.SetNumUninitialized(Voxels);
    for (int32 Pass = 0; Pass < FMath::Max(BlurPasses, 0); ++Pass)
    {
        BlurX(Field, Scratch, Res);
        Swap(Field, Scratch);
        BlurY(Field, Scratch, Res);
        Swap(Field, Scratch);
        BlurZ(Field, Scratch, Res);
        Swap(Field, Scratch);
    }
    Scratch.Empty();

    float Peak = 0.f;
    for (int32 I = 0; I < Voxels; ++I)
    {
        Peak = FMath::Max(Peak, Field[I]);
    }
    if (Peak <= 0.f)
    {
        return false;
    }

    // Percentile over the voxels that hold density. Over a grid that is 98% empty,
    // an all-voxel percentile lands on the faintest blur halo and maps a single
    // point hit to full brightness, so every point renders as a glowing ball.
    constexpr int32 Bins = 256;
    int32 Histogram[Bins] = {};
    int64 NonZero = 0;
    for (int32 I = 0; I < Voxels; ++I)
    {
        if (Field[I] <= 0.f)
        {
            continue;
        }
        const int32 Bin = FMath::Clamp(static_cast<int32>(Field[I] / Peak * (Bins - 1)), 0, Bins - 1);
        ++Histogram[Bin];
        ++NonZero;
    }

    const int64 Target = FMath::Max(NonZero * 95 / 100, 1LL);
    int64 Accumulated = 0;
    int32 CutBin = Bins - 1;
    for (int32 Bin = 0; Bin < Bins; ++Bin)
    {
        Accumulated += Histogram[Bin];
        if (Accumulated >= Target)
        {
            CutBin = Bin;
            break;
        }
    }

    const float Cut = FMath::Max(Peak * (CutBin + 1) / Bins, Peak * 0.05f);
    const float Scale = 255.f / Cut;

    Out.Resolution = Res;
    Out.Bounds = Bounds;
    Out.NumPoints = Points.Num();
    Out.PeakDensity = Peak;
    Out.Voxels.SetNumUninitialized(Voxels * 4);

    ParallelFor(Voxels, [&](int32 Index)
    {
        // The colour its point wrote, not a neighbourhood average: the gradient LUT
        // gives adjacent points different hues, and averaging them collapsed the
        // cloud's colours toward off-white.
        const uint32 Packed = ColorPacked[Index];
        const uint8 Alpha = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Field[Index] * Scale), 0, 255));

        uint8* Bytes = &Out.Voxels[Index * 4];
        Bytes[0] = static_cast<uint8>(Packed & 0xFF);
        Bytes[1] = static_cast<uint8>((Packed >> 8) & 0xFF);
        Bytes[2] = static_cast<uint8>((Packed >> 16) & 0xFF);
        Bytes[3] = Alpha;
    });

    return true;
}
