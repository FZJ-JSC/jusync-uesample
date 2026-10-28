#include "JusyncHUDIcons.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
    constexpr int32 IconSize = 32;
    constexpr int32 Supersample = 4;
    constexpr int32 CanvasSize = IconSize * Supersample;
    constexpr int32 IconCacheVersion = 3;

    struct FIconCanvas
    {
        TArray<uint8> Coverage;

        FIconCanvas()
        {
            Coverage.SetNumZeroed(CanvasSize * CanvasSize);
        }

        void Plot(int32 X, int32 Y)
        {
            if (X >= 0 && X < CanvasSize && Y >= 0 && Y < CanvasSize)
            {
                Coverage[Y * CanvasSize + X] = 255;
            }
        }

        void Clear(int32 X, int32 Y)
        {
            if (X >= 0 && X < CanvasSize && Y >= 0 && Y < CanvasSize)
            {
                Coverage[Y * CanvasSize + X] = 0;
            }
        }

        void FillCircle(float CenterX, float CenterY, float Radius)
        {
            const float CX = CenterX * Supersample;
            const float CY = CenterY * Supersample;
            const float R = Radius * Supersample;
            const int32 MinX = FMath::FloorToInt(CX - R);
            const int32 MaxX = FMath::CeilToInt(CX + R);
            const int32 MinY = FMath::FloorToInt(CY - R);
            const int32 MaxY = FMath::CeilToInt(CY + R);
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float DX = X + 0.5f - CX;
                    const float DY = Y + 0.5f - CY;
                    if (DX * DX + DY * DY <= R * R)
                    {
                        Plot(X, Y);
                    }
                }
            }
        }

        void ClearCircle(float CenterX, float CenterY, float Radius)
        {
            const float CX = CenterX * Supersample;
            const float CY = CenterY * Supersample;
            const float R = Radius * Supersample;
            const int32 MinX = FMath::FloorToInt(CX - R);
            const int32 MaxX = FMath::CeilToInt(CX + R);
            const int32 MinY = FMath::FloorToInt(CY - R);
            const int32 MaxY = FMath::CeilToInt(CY + R);
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float DX = X + 0.5f - CX;
                    const float DY = Y + 0.5f - CY;
                    if (DX * DX + DY * DY <= R * R)
                    {
                        Clear(X, Y);
                    }
                }
            }
        }

        void FillRoundedBox(float X0, float Y0, float X1, float Y1, float Radius)
        {
            const float Left = X0 * Supersample;
            const float Top = Y0 * Supersample;
            const float Right = X1 * Supersample;
            const float Bottom = Y1 * Supersample;
            const float R = Radius * Supersample;
            const float CX = (Left + Right) * 0.5f;
            const float CY = (Top + Bottom) * 0.5f;
            const float HX = (Right - Left) * 0.5f;
            const float HY = (Bottom - Top) * 0.5f;
            const int32 MinX = FMath::FloorToInt(Left);
            const int32 MaxX = FMath::CeilToInt(Right);
            const int32 MinY = FMath::FloorToInt(Top);
            const int32 MaxY = FMath::CeilToInt(Bottom);
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float PX = X + 0.5f;
                    const float PY = Y + 0.5f;
                    const float DX = FMath::Abs(PX - CX) - HX + R;
                    const float DY = FMath::Abs(PY - CY) - HY + R;
                    const float OX = FMath::Max(DX, 0.0f);
                    const float OY = FMath::Max(DY, 0.0f);
                    const float Distance = FMath::Sqrt(OX * OX + OY * OY)
                        + FMath::Min(FMath::Max(DX, DY), 0.0f) - R;
                    if (Distance <= 0.0f)
                    {
                        Plot(X, Y);
                    }
                }
            }
        }

        void ClearRoundedBox(float X0, float Y0, float X1, float Y1, float Radius)
        {
            const float Left = X0 * Supersample;
            const float Top = Y0 * Supersample;
            const float Right = X1 * Supersample;
            const float Bottom = Y1 * Supersample;
            const float R = Radius * Supersample;
            const float CX = (Left + Right) * 0.5f;
            const float CY = (Top + Bottom) * 0.5f;
            const float HX = (Right - Left) * 0.5f;
            const float HY = (Bottom - Top) * 0.5f;
            const int32 MinX = FMath::FloorToInt(Left);
            const int32 MaxX = FMath::CeilToInt(Right);
            const int32 MinY = FMath::FloorToInt(Top);
            const int32 MaxY = FMath::CeilToInt(Bottom);
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float PX = X + 0.5f;
                    const float PY = Y + 0.5f;
                    const float DX = FMath::Abs(PX - CX) - HX + R;
                    const float DY = FMath::Abs(PY - CY) - HY + R;
                    const float OX = FMath::Max(DX, 0.0f);
                    const float OY = FMath::Max(DY, 0.0f);
                    const float Distance = FMath::Sqrt(OX * OX + OY * OY)
                        + FMath::Min(FMath::Max(DX, DY), 0.0f) - R;
                    if (Distance <= 0.0f)
                    {
                        Clear(X, Y);
                    }
                }
            }
        }

        void Line(float X0, float Y0, float X1, float Y1, float Width)
        {
            const float AX = X0 * Supersample;
            const float AY = Y0 * Supersample;
            const float BX = X1 * Supersample;
            const float BY = Y1 * Supersample;
            const float HalfWidth = Width * Supersample * 0.5f;
            const float DX = BX - AX;
            const float DY = BY - AY;
            const float LengthSquared = DX * DX + DY * DY;
            const int32 MinX = FMath::FloorToInt(FMath::Min(AX, BX) - HalfWidth);
            const int32 MaxX = FMath::CeilToInt(FMath::Max(AX, BX) + HalfWidth);
            const int32 MinY = FMath::FloorToInt(FMath::Min(AY, BY) - HalfWidth);
            const int32 MaxY = FMath::CeilToInt(FMath::Max(AY, BY) + HalfWidth);
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float PX = X + 0.5f - AX;
                    const float PY = Y + 0.5f - AY;
                    const float T = LengthSquared > 0.0f
                        ? FMath::Clamp((PX * DX + PY * DY) / LengthSquared, 0.0f, 1.0f)
                        : 0.0f;
                    const float QX = PX - DX * T;
                    const float QY = PY - DY * T;
                    if (QX * QX + QY * QY <= HalfWidth * HalfWidth)
                    {
                        Plot(X, Y);
                    }
                }
            }
        }

        void Arc(float CenterX, float CenterY, float Radius, float StartDegrees,
            float EndDegrees, float Width)
        {
            const float CX = CenterX * Supersample;
            const float CY = CenterY * Supersample;
            const float R = Radius * Supersample;
            const float HalfWidth = Width * Supersample * 0.5f;
            const int32 MinX = FMath::FloorToInt(CX - R - HalfWidth);
            const int32 MaxX = FMath::CeilToInt(CX + R + HalfWidth);
            const int32 MinY = FMath::FloorToInt(CY - R - HalfWidth);
            const int32 MaxY = FMath::CeilToInt(CY + R + HalfWidth);

            auto NormalizeDegrees = [](float Degrees)
            {
                float Result = FMath::Fmod(Degrees, 360.0f);
                return Result < 0.0f ? Result + 360.0f : Result;
            };
            const float Start = NormalizeDegrees(StartDegrees);
            float End = NormalizeDegrees(EndDegrees);
            if (End < Start)
            {
                End += 360.0f;
            }

            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const float DX = X + 0.5f - CX;
                    const float DY = Y + 0.5f - CY;
                    const float Distance = FMath::Sqrt(DX * DX + DY * DY);
                    if (FMath::Abs(Distance - R) > HalfWidth)
                    {
                        continue;
                    }

                    float Angle = FMath::RadiansToDegrees(FMath::Atan2(DY, DX));
                    if (Angle < 0.0f)
                    {
                        Angle += 360.0f;
                    }
                    if (Angle < Start)
                    {
                        Angle += 360.0f;
                    }
                    if (Angle >= Start && Angle <= End)
                    {
                        Plot(X, Y);
                    }
                }
            }
        }

        void Triangle(float AX, float AY, float BX, float BY, float CX, float CY)
        {
            const FVector2D A(AX * Supersample, AY * Supersample);
            const FVector2D B(BX * Supersample, BY * Supersample);
            const FVector2D C(CX * Supersample, CY * Supersample);
            const int32 MinX = FMath::FloorToInt(FMath::Min3(A.X, B.X, C.X));
            const int32 MaxX = FMath::CeilToInt(FMath::Max3(A.X, B.X, C.X));
            const int32 MinY = FMath::FloorToInt(FMath::Min3(A.Y, B.Y, C.Y));
            const int32 MaxY = FMath::CeilToInt(FMath::Max3(A.Y, B.Y, C.Y));
            auto Edge = [](const FVector2D& P0, const FVector2D& P1, const FVector2D& P)
            {
                return (P.X - P0.X) * (P1.Y - P0.Y) - (P.Y - P0.Y) * (P1.X - P0.X);
            };
            for (int32 Y = MinY; Y <= MaxY; ++Y)
            {
                for (int32 X = MinX; X <= MaxX; ++X)
                {
                    const FVector2D P(X + 0.5f, Y + 0.5f);
                    const float E0 = Edge(A, B, P);
                    const float E1 = Edge(B, C, P);
                    const float E2 = Edge(C, A, P);
                    const bool bInside = (E0 >= 0.0f && E1 >= 0.0f && E2 >= 0.0f)
                        || (E0 <= 0.0f && E1 <= 0.0f && E2 <= 0.0f);
                    if (bInside)
                    {
                        Plot(X, Y);
                    }
                }
            }
        }
    };

    void DrawIcon(FIconCanvas& Canvas, JusyncHUDIcons::EIcon Icon)
    {
        using EIcon = JusyncHUDIcons::EIcon;
        switch (Icon)
        {
        case EIcon::Play:
            Canvas.Triangle(10.0f, 7.0f, 25.0f, 16.0f, 10.0f, 25.0f);
            break;
        case EIcon::Pause:
            Canvas.FillRoundedBox(10.0f, 7.0f, 14.0f, 25.0f, 1.5f);
            Canvas.FillRoundedBox(18.0f, 7.0f, 22.0f, 25.0f, 1.5f);
            break;
        case EIcon::Previous:
            Canvas.FillRoundedBox(8.0f, 8.0f, 12.0f, 24.0f, 1.5f);
            Canvas.Triangle(24.0f, 8.0f, 24.0f, 24.0f, 12.0f, 16.0f);
            break;
        case EIcon::Next:
            Canvas.Triangle(8.0f, 8.0f, 8.0f, 24.0f, 20.0f, 16.0f);
            Canvas.FillRoundedBox(20.0f, 8.0f, 24.0f, 24.0f, 1.5f);
            break;
        case EIcon::Live:
            Canvas.FillCircle(16.0f, 16.0f, 6.0f);
            break;
        case EIcon::Loop:
            Canvas.Arc(16.0f, 16.0f, 8.5f, 40.0f, 320.0f, 2.4f);
            Canvas.Triangle(24.6f, 13.0f, 20.0f, 11.1f, 23.5f, 8.1f);
            break;
        case EIcon::Plus:
            Canvas.Line(10.0f, 16.0f, 22.0f, 16.0f, 3.0f);
            Canvas.Line(16.0f, 10.0f, 16.0f, 22.0f, 3.0f);
            break;
        case EIcon::Minus:
            Canvas.Line(10.0f, 16.0f, 22.0f, 16.0f, 3.0f);
            break;
        case EIcon::Square:
            Canvas.FillRoundedBox(8.0f, 8.0f, 24.0f, 24.0f, 2.5f);
            Canvas.ClearRoundedBox(10.5f, 10.5f, 21.5f, 21.5f, 0.8f);
            break;
        case EIcon::Circle:
            Canvas.FillCircle(16.0f, 16.0f, 8.8f);
            Canvas.ClearCircle(16.0f, 16.0f, 6.2f);
            break;
        case EIcon::ChevronDown:
            Canvas.Line(9.0f, 13.0f, 16.0f, 20.0f, 2.6f);
            Canvas.Line(16.0f, 20.0f, 23.0f, 13.0f, 2.6f);
            break;
        }
    }

    UTexture2D* CreateIconTexture(JusyncHUDIcons::EIcon Icon)
    {
        FIconCanvas Canvas;
        DrawIcon(Canvas, Icon);

        TArray<FColor> Pixels;
        Pixels.SetNumUninitialized(IconSize * IconSize);
        for (int32 Y = 0; Y < IconSize; ++Y)
        {
            for (int32 X = 0; X < IconSize; ++X)
            {
                uint32 Coverage = 0;
                for (int32 SY = 0; SY < Supersample; ++SY)
                {
                    for (int32 SX = 0; SX < Supersample; ++SX)
                    {
                        const int32 SourceX = X * Supersample + SX;
                        const int32 SourceY = Y * Supersample + SY;
                        Coverage += Canvas.Coverage[SourceY * CanvasSize + SourceX];
                    }
                }
                const uint8 Alpha = static_cast<uint8>(
                    (Coverage + (Supersample * Supersample) / 2) / (Supersample * Supersample));
                Pixels[Y * IconSize + X] = FColor(255, 255, 255, Alpha);
            }
        }

        UTexture2D* Texture = UTexture2D::CreateTransient(IconSize, IconSize, PF_B8G8R8A8);
        if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
        {
            return nullptr;
        }

        Texture->SRGB = true;
        Texture->Filter = TF_Bilinear;
        Texture->AddressX = TA_Clamp;
        Texture->AddressY = TA_Clamp;
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->NeverStream = true;

        FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
        void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
        Mip.BulkData.Unlock();
        Texture->UpdateResource();
        return Texture;
    }

    UTexture2D* GetIconTexture(JusyncHUDIcons::EIcon Icon)
    {
        static TMap<int32, TStrongObjectPtr<UTexture2D>> CachedTextures;
        const int32 Key = (IconCacheVersion << 8) | static_cast<int32>(Icon);
        if (TStrongObjectPtr<UTexture2D>* Cached = CachedTextures.Find(Key))
        {
            return Cached->Get();
        }

        UTexture2D* Texture = CreateIconTexture(Icon);
        if (Texture)
        {
            CachedTextures.Emplace(Key, TStrongObjectPtr<UTexture2D>(Texture));
        }
        return Texture;
    }
}

namespace JusyncHUDIcons
{
    UImage* MakeImage(UWidgetTree* Tree, EIcon Icon, float Size, const FLinearColor& Color)
    {
        if (!Tree)
        {
            return nullptr;
        }

        UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), NAME_None);
        Image->SetBrushFromTexture(GetIconTexture(Icon), false);
        Image->SetColorAndOpacity(Color);
        Image->SetDesiredSizeOverride(FVector2D(Size, Size));
        Image->SetVisibility(ESlateVisibility::HitTestInvisible);
        return Image;
    }

    void SetImageIcon(UImage* Image, EIcon Icon)
    {
        if (Image)
        {
            Image->SetBrushFromTexture(GetIconTexture(Icon), false);
        }
    }

    UTexture2D* GetTexture(EIcon Icon)
    {
        return GetIconTexture(Icon);
    }
}
