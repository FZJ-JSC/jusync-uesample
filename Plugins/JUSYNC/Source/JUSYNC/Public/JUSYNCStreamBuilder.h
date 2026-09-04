#pragma once

#include "CoreMinimal.h"
#include "JUSYNCTypes.h"
#include "RealtimeMeshSimple.h"

JUSYNC_API bool JUSYNCBuildRealtimeMeshStreams(const FJUSYNCMeshData& MeshData, RealtimeMesh::FRealtimeMeshStreamSet& OutStreams);
