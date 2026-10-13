#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"
#include "JUSYNCTypes.h"

class URealtimeMeshComponent;
class UMaterialInstanceDynamic;

/**
 * GPU position morphing for streamed time samples (Anari-USD mesh updates).
 *
 * On every refresh the subsystem uploads the NEW sample into the Position
 * stream and the PREVIOUS displayed sample into two extra float32 UV channels
 * (UV1 = prev.xy, UV2 = prev.z). The material's World Position Offset then
 * interpolates between them with one scalar:
 *
 *     WPO = JusyncMorphActive * (1 - JusyncMorphT) * (TransformPos(UV12) - AbsWorldPos)
 *
 * CPU cost per rendered frame is a single SetScalarParameterValue per morphing
 * component; per SAMPLE arrival it is two position-array copies (prev bake +
 * record current). Meshes whose material lacks the JusyncMorphT parameter fall
 * back to the existing instant-apply path with zero behavior change.
 *
 * Point-count changes between samples are handled by lerping the overlapping
 * index prefix and holding born points at their own position; triangle-index
 * changes (remesh) snap instead of crossfading.
 *
 * Owned by UJUSYNCSubsystem; game-thread only (FTickableGameObject).
 */
class JUSYNC_API FJUSYNCMeshMorpher : public FTickableGameObject
{
public:
    /** Material parameters consumed by the morph WPO branch. */
    static const FName MorphTParameter;        // "JusyncMorphT"
    static const FName MorphActiveParameter;   // "JusyncMorphActive"

    explicit FJUSYNCMeshMorpher(TWeakObjectPtr<UObject> InOwner);

    /**
     * Decide how MeshData should be uploaded for this component refresh.
     * IntentMaterial is the material that will render this update (already a
     * dynamic instance is fine); blending requires the JusyncMorphT parameter.
     * EApplyMode::Blend  -> caller must build streams with JUSYNCBuildRealtimeMeshStreams(MeshData,
     *                       Streams, &OutPrevPoints) and call Commit() after the upload.
     * EApplyMode::Normal -> caller uploads as before (no prev channels); Commit() still records
     *                       state so the NEXT sample can blend.
     */
    enum class EApplyMode : uint8 { Normal, Blend };

    EApplyMode BeginUpdate(URealtimeMeshComponent* Comp, UMaterialInterface* IntentMaterial,
                           const FJUSYNCCompactMeshData& MeshData,
                           double NowSeconds, TArray<FVector3f>& OutPrevPoints);

    /** Record the uploaded sample; starts the GPU ramp when bBlended. UploadedPrev is the
     *  array passed to the stream builder (moved into the entry as the GPU-side prev). */
    void Commit(URealtimeMeshComponent* Comp, const FJUSYNCCompactMeshData& MeshData,
                double NowSeconds, bool bBlended, TArray<FVector3f>&& UploadedPrev);

    /** Seed state for a freshly spawned mesh so its first update can blend. */
    void Seed(URealtimeMeshComponent* Comp, const FJUSYNCCompactMeshData& MeshData, double NowSeconds);

    /** Forget a component (actor teardown hook; stale entries also self-prune in Tick). */
    void Forget(URealtimeMeshComponent* Comp);

    bool IsEnabled() const;

    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FJUSYNCMeshMorpher, STATGROUP_Tickables); }
#if WITH_EDITOR
    virtual bool IsTickableInEditor() const override { return true; }
#endif

private:
    struct FEntry
    {
        /** Displayed state the GPU currently holds in the prev channels. */
        TArray<FVector3f> PrevPoints;
        /** Points of the last uploaded sample (for mid-flight retarget baking). */
        TArray<FVector3f> CurrPoints;
        /** CityHash of the last uploaded triangle index block (topology guard). */
        uint64 TriHash = 0;

        double LastArrival = 0.0;
        double BlendStart = 0.0;
        double Duration = 0.0;
        bool bBlending = false;
        bool bHasTriHash = false;

        TWeakObjectPtr<UMaterialInstanceDynamic> MorphMID;
    };

    static uint64 ComputeTriHash(const TArray<int32>& Triangles);
    static bool IsMorphCapable(const UMaterialInterface* Material);

    /** Ensure the component renders with a morph-parameterized MID; returns null when not capable. */
    UMaterialInstanceDynamic* ResolveMorphMID(URealtimeMeshComponent* Comp, FEntry& Entry);

    TWeakObjectPtr<UObject> Owner;
    TMap<TWeakObjectPtr<URealtimeMeshComponent>, FEntry> Entries;
};
