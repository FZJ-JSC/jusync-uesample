#pragma once

#include "CoreMinimal.h"
#include "Components/BackgroundBlur.h"
#include "Components/Widget.h"

struct FJusyncGlassBlur
{
	TWeakObjectPtr<UBackgroundBlur> Blur;
	float Strength = 0.0f;
};

struct FJusyncPanelReveal
{
	static constexpr float DefaultOpenSeconds = 0.30f;
	static constexpr float DefaultCloseSeconds = 0.20f;

	float Alpha = 0.0f;

	void Show(float Seconds = DefaultOpenSeconds)
	{
		Begin(1.0f, Seconds);
	}

	void Hide()
	{
		Begin(0.0f, DefaultCloseSeconds);
	}

	bool Tick(float DeltaSeconds)
	{
		if (!bAnimating)
		{
			return false;
		}

		Elapsed += FMath::Max(DeltaSeconds, 0.0f);
		const float T = Duration > KINDA_SMALL_NUMBER
			? FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f)
			: 1.0f;

		const float Eased = Target > FromAlpha
			? 1.0f - FMath::Pow(1.0f - T, 5.0f)
			: T * T * T;

		Alpha = FMath::Lerp(FromAlpha, Target, Eased);

		if (T >= 1.0f)
		{
			Alpha = Target;
			bAnimating = false;
		}
		return true;
	}

	bool IsClosed() const
	{
		return !bAnimating && Alpha <= KINDA_SMALL_NUMBER;
	}

private:
	void Begin(float InTarget, float Seconds)
	{
		if (bAnimating && FMath::IsNearlyEqual(Target, InTarget))
		{
			return;
		}

		FromAlpha = Alpha;
		Target = InTarget;
		Elapsed = 0.0f;
		Duration = FMath::Max(Seconds, 0.01f);
		bAnimating = true;
	}

	bool bAnimating = false;
	float FromAlpha = 0.0f;
	float Target = 0.0f;
	float Elapsed = 0.0f;
	float Duration = 0.0f;
};

inline void JusyncApplyPanelReveal(const FJusyncPanelReveal& Reveal, UWidget* Root,
	const FVector2D& SlideFrom, const TArray<FJusyncGlassBlur>& Glass)
{
	const float A = FMath::Clamp(Reveal.Alpha, 0.0f, 1.0f);

	if (Root)
	{
		Root->SetRenderOpacity(A);
		Root->SetRenderTranslation(SlideFrom * (1.0f - A));
	}

	for (const FJusyncGlassBlur& Entry : Glass)
	{
		if (UBackgroundBlur* Blur = Entry.Blur.Get())
		{
			Blur->SetBlurStrength(Entry.Strength * A);
		}
	}
}
