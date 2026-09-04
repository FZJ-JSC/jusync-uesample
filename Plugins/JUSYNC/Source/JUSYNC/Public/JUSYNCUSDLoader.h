#pragma once

#include "CoreMinimal.h"
#include "JUSYNCTypes.h"
#include "JUSYNCStreamBuilder.h"

class UJUSYNCSubsystem;

struct FJUSYNCParsedFileResult
{
    FString Filename;
    int32 FileIndex = -1;
    int32 Rank = 0;
    uint64 Generation = 0;
    uint64 HashLo = 0;
    uint64 HashHi = 0;
    int64 Size = 0;
    bool bParsed = false;
    uint64 LUTVersion = 0;
    TArray<FJUSYNCMeshData> Meshes;
    TArray<FJUSYNCPointCloudData> PointClouds;
    TArray<TUniquePtr<RealtimeMesh::FRealtimeMeshStreamSet>> PrebuiltStreams;
};

class JUSYNC_API FJUSYNCUSDLoader
{
public:
    static void BakeLUTIntoMesh(FJUSYNCMeshData& Mesh, const TArray<FColor>& LUT);
    static void BakeLUTIntoMeshes(TArray<FJUSYNCMeshData>& Meshes, const TArray<FColor>& LUT);

    static bool ParseBuffer(UJUSYNCSubsystem* Subsystem, const TArray<uint8>& Buffer, const FString& Filename,
        TArray<FJUSYNCMeshData>& OutMeshes, TArray<FJUSYNCPointCloudData>& OutPointClouds);

    /**
     * Task-graph parse path. The USD layout query is still serialized inside the
     * subsystem (tinyusdz constraint), but buffer ownership, LUT baking, and the
     * game-thread hand-off are scheduled through TaskGraph.
     */
    static void ParseAsync(
        UJUSYNCSubsystem* Subsystem,
        TArray<uint8>&& Buffer,
        FString Filename,
        TArray<FColor> LUT,
        uint64 LUTVersion,
        bool bPrebuildStreams,
        int32 FileIndex,
        int32 Rank,
        uint64 Generation,
        uint64 HashLo,
        uint64 HashHi,
        int64 Size,
        TFunction<void(FJUSYNCParsedFileResult&&)>&& OnGameThread);
};
