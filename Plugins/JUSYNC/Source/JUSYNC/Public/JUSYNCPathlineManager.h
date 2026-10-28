#pragma once

#include "CoreMinimal.h"
#include "JUSYNCTypes.h"
#include "UObject/SoftObjectPtr.h"

class UMaterialInterface;
class UNiagaraSystem;

/**
 * User-facing pathline controls. These mirror the details panel controls on
 * AJUSYNCFileSpawnerActor; the manager itself has no UI.
 */
struct JUSYNC_API FJUSYNCPathlineSettings
{
	bool bEnabled = false;

	int32 MaxTracks = 4096;
	int32 HistorySamples = 128;
	// One ribbon draws at most HistorySamples nodes, but a track keeps up to
	// RetentionSamples so the display cutoff can be rewound through the cached
	// timeline. Clamped by MaxNodes at ingest.
	int32 RetentionSamples = 256;
	int32 MaxNodes = 524288;
	// A track that is not present in N consecutive emitted samples is removed.
	int32 TrackExpirySamples = 32;

	/**
	 * Niagara system that draws the trails as ribbons. When null nothing is
	 * drawn; the CPU track store still ingests so picking and scrubbing stay
	 * valid.
	 */
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

	// Colour picker filter: keeps tracks whose resolved (rendered) colour
	// lies within HueTolerance degrees of Hue on the HSV wheel. Independent
	// of the scalar-range filter; when both are on, a track must pass both.
	bool bFilterByHue = false;
	float FilterHue = 0.0f;          // degrees, 0-360
	float FilterHueTolerance = 30.0f; // degrees

	bool bColorByScalar = true;
	float ColorMin = 0.0f;
	float ColorMax = 1.0f;
	FLinearColor FlatColor = FLinearColor::White;

	// Stable hash rank. Changing the seed re-samples which IDs are tracked.
	uint64 SelectionSeed = 1337;

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

/**
 * Ribbon particle data for one element, ready to be pushed into Niagara.
 *
 * Every array has the same length: one entry per emitted ribbon point. The
 * ribbon renderer joins the points of one element in LinkOrder sequence, so a
 * single track that breaks into several runs is emitted as one polyline with
 * zero-width duplicate points bridging the gaps.
 *
 * Plain data only (no UObjects): built on the worker thread, consumed on the
 * game thread.
 */
struct JUSYNC_API FJUSYNCPathlineArrays
{
	TArray<FVector> Positions;
	TArray<float> LinkOrders;
	TArray<float> Widths;
	TArray<FLinearColor> Colors;
	/** Normalised 0-1 scalar per point, for materials that sample a LUT. */
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

/**
 * CPU pathline store and ribbon array builder.
 *
 * Ingestion and geometry construction run on a serialized background worker.
 * The game thread only uploads finished arrays (see DrainArrays). Tests can
 * use IngestSampleNow/BuildArrays directly without starting the worker.
 *
 * Threading contract: the point-cloud and LUT arguments are copied into the
 * work item. FJUSYNCPointCloudData is treated as immutable after enqueue.
 */
class JUSYNC_API FJUSYNCPathlineManager
{
public:
	FJUSYNCPathlineManager();
	~FJUSYNCPathlineManager();

	FJUSYNCPathlineManager(const FJUSYNCPathlineManager&) = delete;
	FJUSYNCPathlineManager& operator=(const FJUSYNCPathlineManager&) = delete;

	void SetSettings(const FJUSYNCPathlineSettings& NewSettings);
	const FJUSYNCPathlineSettings& GetSettings() const;

	// True when structural settings changed and the file spawner should replay
	// its retained samples through the manager.
	bool ConsumeResampleRequest();

	void Reset();

	// Drops all retained state for one canonical element. Called when the last
	// point-cloud rank for that element is retired; the spawner removes the
	// matching actor on its next pass.
	void RemoveElement(const FString& ElementName);

	void EnqueueSample(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut);
	void IngestSampleNow(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut);

	// Builds ribbon arrays for one canonical element at a sample cutoff.
	// MAX_uint64 means "live". Returns false when the element has no state.
	bool BuildArrays(const FString& ElementName, uint64 CutoffSampleId,
		FJUSYNCPathlineArrays& OutArrays, int32& OutVisibleTracks);

	void SetDisplaySampleId(uint64 SampleId);
	uint64 GetDisplaySampleId() const;

	/**
	 * Ribbon tip smoothing for animated playback. While the display cutoff keeps
	 * advancing, the newest node of every trail glides toward its next sample
	 * over this window instead of snapping once per cached frame. 0 restores
	 * exact, per-sample snapshots (scrubs). Call TickDisplayBlend() every frame
	 * while animating.
	 */
	void SetDisplayTipBlendSeconds(float Seconds);
	void TickDisplayBlend();

	// Queue a rebuild from retained state (playback scrub or visual change).
	void RequestRebuild();

	/**
	 * One finished array set, handed to the game thread for upload. Purely
	 * data: the manager never touches UObjects, the caller owns the Niagara
	 * components.
	 */
	struct FCompletedArrays
	{
		FString ElementName;
		TSharedPtr<FJUSYNCPathlineArrays, ESPMode::ThreadSafe> Arrays;
	};

	/** Drains arrays finished by the worker since the last call. */
	void DrainArrays(TArray<FCompletedArrays>& OutArrays);

	FJUSYNCPathlineStats GetStats() const;

	TArray<FString> GetElementKeys() const;

	// Deterministic selection inspection (tests/tools).
	TArray<uint64> GetTrackIds(const FString& ElementName, int32 Rank) const;
	int32 GetElementNodeCount(const FString& ElementName) const;
	int32 GetTrackNodeCount(const FString& ElementName, int32 Rank, uint64 Id) const;

	/**
	 * ParaView's VTK exporter prefixes point arrays on the wire (a user array
	 * "particle_id" arrives as "vtk_particle_id"). Returns the prefixed name
	 * when only that exists on the cloud, otherwise Name unchanged, so the
	 * configured attribute names keep working for both sender families.
	 */
	static FString ResolveAttributeName(const FJUSYNCPointCloudData& Cloud, const FString& Name);

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
