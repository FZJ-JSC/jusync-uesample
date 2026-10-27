#pragma once

#include "CoreMinimal.h"
#include "RealtimeMeshSimple.h"

/**
 * True when GroupKey already exists in the mesh, without tripping an ensure().
 *
 * URealtimeMeshSimple::GetSectionGroup() must not be used as an existence test: it runs
 * through FRealtimeMeshAccessor::AddSectionGroupTask, which raises ensure() plus a
 * MessageLog error whenever the group is missing. "Missing" is the normal case here (first
 * frame of a mesh, and every frame right after ClearMesh()), and each hit costs seconds -
 * the editor log measured FDebug::EnsureFailed at ~2.1s per occurrence, which reads as a
 * freeze the moment pathline actors start appearing.
 *
 * GetSectionGroups() only ensures when the LOD itself is absent, so rule that out first
 * with GetLODs() (that one goes through AddMeshTask and never ensures).
 */
inline bool JUSYNCRealtimeMeshHasSectionGroup(
	const URealtimeMeshSimple* RealtimeMesh, const FRealtimeMeshSectionGroupKey& GroupKey)
{
	if (!RealtimeMesh)
	{
		return false;
	}

	const FRealtimeMeshLODKey LODKey = GroupKey.LOD();
	if (!RealtimeMesh->GetLODs().Contains(LODKey))
	{
		return false;
	}

	return RealtimeMesh->GetSectionGroups(LODKey).Contains(GroupKey);
}
