#pragma once

#include "CoreMinimal.h"
#include "JUSYNCTypes.h"
#include "UObject/SoftObjectPtr.h"

class UMaterialInterface;
class UNiagaraSystem;

struct JUSYNC_API FJUSYNCPathlineSettings
{
	bool bEnabled = false;

	int32 MaxTracks = 4096;
	int32 HistorySamples = 128;
	int32 RetentionSamples = 256;
	int32 MaxNodes = 524288;
	int32 TrackExpirySamples = 32;

	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	float Radius = 0.5f;
	float RadiusScale = 1.0f;
	bool bUsePointWidth = false;

	FString IdAttribute = TEXT("particle_id");
	FString ScalarAttribute = TEXT("attribute0");
	int32 ScalarComponent = 0;

	bool bFilterEnabled = false;
	float FilterMin = 0.0f;
	float FilterMax = 1.0f;

	bool bFilterByHue = false;
	float FilterHue = 0.0f;  // degrees, 0-360
	float FilterHueTolerance = 30.0f;  // degrees

	bool bColorByScalar = true;
	float ColorMin = 0.0f;
	float ColorMax = 1.0f;
	FLinearColor FlatColor = FLinearColor::White;

	uint64 SelectionSeed = 1337;

	// Visual liveliness. A newly seen track grows from its tail to its tip over
	// RevealSeconds, and RevealStaggerSeconds spreads that across tracks so a
	// fresh dataset does not pop in all at once like a stamp.
	float RevealSeconds = 1.1f;
	float RevealStaggerSeconds = 0.7f;
	// Fast segments thin the ribbon and sharp turns brighten it, which reads as
	// motion rather than as a drawing. TailTaper is the width at the oldest
	// point as a fraction of the head width.
	float SpeedWidthScale = 0.55f;
	float CornerBoost = 2.5f;
	float TailTaper = 0.12f;

	// A bright band sweeps from the oldest sample to the newest one, so a
	// standing trail still reads as flow. FlowSpeed is in bands per second,
	// FlowCycles is how many bands span one trail, and FlowSharpness shapes
	// the comet: higher values give a thin head with a long fading tail.
	float FlowSpeed = 0.15f;
	float FlowCycles = 1.0f;
	float FlowSharpness = 3.5f;
	float FlowStrength = 6.0f;

	// A trail is only a dozen history points long, so shading the band once per
	// point makes the head step from point to point. Each history segment is
	// split into SegmentSubdiv pieces before shading to give the band a frame
	// it can move through smoothly.
	int32 SegmentSubdiv = 6;
	// Width of the comet head as a multiple of the trail width, and how much of
	// the loop it spans. Widest just behind the head and converging to a point
	// at the head itself, which reads as an arrow head. 0 disables it.
	float CometArrowWidth = 3.0f;
	float CometArrowSpan = 0.10f;

	UMaterialInterface* Material = nullptr;
};

struct JUSYNC_API FJUSYNCPathlineStats
{
	int32 ActiveTracks = 0;
	int64 TotalNodes = 0;
	int32 VisibleTracks = 0;
	int64 DuplicateIds = 0;
	int64 DroppedSamples = 0;
	int64 IgnoredSamples = 0;
	double LastIngestMs = 0.0;
	double LastBuildMs = 0.0;
};

struct JUSYNC_API FJUSYNCPathlineArrays
{
	TArray<FVector> Positions;
	TArray<float> LinkOrders;
	TArray<float> Widths;
	TArray<FLinearColor> Colors;
	TArray<float> Scalars;

	int32 Num() const { return Positions.Num(); }

	void Reset()
	{
		Positions.Reset();
		LinkOrders.Reset();
		Widths.Reset();
		Colors.Reset();
		Scalars.Reset();
	}

	void Reserve(int32 Count)
	{
		Positions.Reserve(Count);
		LinkOrders.Reserve(Count);
		Widths.Reserve(Count);
		Colors.Reserve(Count);
		Scalars.Reserve(Count);
	}
};

class JUSYNC_API FJUSYNCPathlineManager
{
public:
	FJUSYNCPathlineManager();
	~FJUSYNCPathlineManager();

	FJUSYNCPathlineManager(const FJUSYNCPathlineManager&) = delete;
	FJUSYNCPathlineManager& operator=(const FJUSYNCPathlineManager&) = delete;

	void SetSettings(const FJUSYNCPathlineSettings& NewSettings);
	const FJUSYNCPathlineSettings& GetSettings() const;

	bool ConsumeResampleRequest();

	void Reset();

	void RemoveElement(const FString& ElementName);

	void EnqueueSample(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut);
	void IngestSampleNow(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut);

	bool BuildArrays(const FString& ElementName, uint64 CutoffSampleId,
		FJUSYNCPathlineArrays& OutArrays, int32& OutVisibleTracks);

	void SetDisplaySampleId(uint64 SampleId);
	uint64 GetDisplaySampleId() const;

	void SetDisplayTipBlendSeconds(float Seconds);
	void TickDisplayBlend();

	void RequestRebuild();

	struct FCompletedArrays
	{
		FString ElementName;
		TSharedPtr<FJUSYNCPathlineArrays, ESPMode::ThreadSafe> Arrays;
	};

	void DrainArrays(TArray<FCompletedArrays>& OutArrays);

	FJUSYNCPathlineStats GetStats() const;

	TArray<FString> GetElementKeys() const;

	TArray<uint64> GetTrackIds(const FString& ElementName, int32 Rank) const;
	int32 GetElementNodeCount(const FString& ElementName) const;
	int32 GetTrackNodeCount(const FString& ElementName, int32 Rank, uint64 Id) const;

	static FString ResolveAttributeName(const FJUSYNCPointCloudData& Cloud, const FString& Name);

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
