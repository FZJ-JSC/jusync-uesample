#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JUSYNCVolumeBuilder.h"
#include "JUSYNCVolumeActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextureRenderTargetVolume;

UCLASS()
class JUSYNC_API AJUSYNCVolumeActor : public AActor
{
    GENERATED_BODY()

public:
    AJUSYNCVolumeActor();

    void ApplyGrid(const FJUSYNCVolumeGrid& Grid, UMaterialInterface* InMaterial);

    UStaticMeshComponent* GetBox() const { return Box; }

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "JUSYNC")
    TObjectPtr<UStaticMeshComponent> Box;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTargetVolume> VolumeTarget;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> VolumeMaterial;

    int32 TextureSize = 0;

    bool bGridStatsLogged = false;

    // Edge length of one voxel in the current grid; extinction is quoted per voxel.
    FVector LastVoxelSize = FVector(1.0);

    // Signature of the tuning cvars, to notice a change between grid updates.
    uint32 ParameterSignature = 0;

    void EnsureResources(int32 Resolution, UMaterialInterface* InMaterial);
    void UploadVoxels(const FJUSYNCVolumeGrid& Grid);
    void PushMaterialParameters();
};
