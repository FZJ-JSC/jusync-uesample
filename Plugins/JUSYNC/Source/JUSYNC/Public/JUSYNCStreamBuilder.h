#pragma once

#include "CoreMinimal.h"
#include "JUSYNCTypes.h"
#include "RealtimeMeshSimple.h"

JUSYNC_API bool JUSYNCBuildRealtimeMeshStreams(const FJUSYNCMeshData& MeshData, RealtimeMesh::FRealtimeMeshStreamSet& OutStreams);

JUSYNC_API bool JUSYNCBuildRealtimeMeshStreams(const FJUSYNCCompactMeshData& MeshData, RealtimeMesh::FRealtimeMeshStreamSet& OutStreams);

/**
 * GPU-morph variant: when PrevPositions is supplied, the TexCoords stream is built as
 * three float32 channels per vertex: UV0 = real UVs, UV1 = prev.xy, UV2 = (prev.z, 0).
 * Missing prev entries (born points) repeat the vertex's own position so the material
 * blend produces zero displacement for them. Requires a morph-capable material.
 */
JUSYNC_API bool JUSYNCBuildRealtimeMeshStreams(const FJUSYNCCompactMeshData& MeshData, RealtimeMesh::FRealtimeMeshStreamSet& OutStreams, const TArray<FVector3f>* PrevPositions);
