#pragma once

#include "CoreMinimal.h"
#include "Components/BackgroundBlur.h"
#include "Components/Widget.h"

/**
 * One frosted surface of the runtime HUD. The strength is captured when the
 * layer is built, because the reveal fades the blur from 0 to that value and
 * the widget no longer knows the authored number once it has been scaled.
 */
struct FJusyncGlassBlur
{
	TWeakObjectPtr<UBackgroundBlur> Blur;
	float Strength = 0.0f;
};

/**
 * Eased show/hide used by every floating HUD surface: the transport deck, the
 * pathline panel and the shortcut legend.
 *
 * Alpha is the whole animation state. Panels drive their root opacity and a
 * small slide from it, and scale each blur strength by it, so the glass grows
 * in instead of popping in as a hard rectangle. Reversing mid-flight simply
 * starts a new tween from the current alpha, which keeps rapid T presses soft.
 *
 * Collapsed widgets do not tick, so the owning widget has to make itself
 * visible before Show() and may only collapse itself once IsClosed().
 */
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

	/** Returns true while the value changed (i.e. the panel needs a repaint). */
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

		// Opening lands with a long, soft ease-out; closing leaves quickly so
		// the panel does not sit half transparent over the scene.
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

/**
 * Applies a reveal to a panel root and its glass layers. SlideFrom is the
 * offset the panel starts at while hidden; scaling it by (1 - Alpha) makes the
 * surface drift into place as it fades, which reads as a rise rather than a
 * cross-fade.
 */
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
