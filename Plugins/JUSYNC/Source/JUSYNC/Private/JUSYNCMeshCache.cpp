#include "JUSYNCMeshCache.h"

FJUSYNCMeshCache::FJUSYNCMeshCache()
{
}

FJUSYNCMeshCache::~FJUSYNCMeshCache()
{
}

FString FJUSYNCMeshCache::MakeKey(const FString& Filename, uint64 HashLo, uint64 HashHi, int64 Size)
{
    if (HashLo != 0 || HashHi != 0)
    {
        return FString::Printf(TEXT("%s|H:%llx-%llx|S:%lld"), *Filename,
            static_cast<unsigned long long>(HashLo), static_cast<unsigned long long>(HashHi), static_cast<long long>(Size));
    }
    return FString::Printf(TEXT("%s|S:%lld"), *Filename, static_cast<long long>(Size));
}

int64 FJUSYNCMeshCache::EstimatePayloadBytes(const TArray<FJUSYNCMeshData>& Meshes, const TArray<FJUSYNCPointCloudData>& PointClouds)
{
    int64 Bytes = 0;
    for (const FJUSYNCMeshData& M : Meshes)
    {
        Bytes += 128;
        Bytes += static_cast<int64>(M.Vertices.Num()) * sizeof(FVector);
        Bytes += static_cast<int64>(M.Normals.Num()) * sizeof(FVector);
        Bytes += static_cast<int64>(M.UVs.Num()) * sizeof(FVector2D);
        Bytes += static_cast<int64>(M.VertexColors.Num()) * sizeof(FColor);
        Bytes += static_cast<int64>(M.Triangles.Num()) * sizeof(int32);
    }
    for (const FJUSYNCPointCloudData& P : PointClouds)
    {
        Bytes += 128;
        Bytes += static_cast<int64>(P.Positions.Num()) * sizeof(FVector);
        Bytes += static_cast<int64>(P.Colors.Num()) * sizeof(FColor);
        Bytes += static_cast<int64>(P.Widths.Num()) * sizeof(float);
    }
    return Bytes;
}

bool FJUSYNCMeshCache::HasBakedVertexColors(const TArray<FJUSYNCMeshData>& Meshes)
{
    for (const FJUSYNCMeshData& M : Meshes)
    {
        if (M.VertexColors.Num() > 0)
        {
            return true;
        }
    }
    return false;
}

bool FJUSYNCMeshCache::Contains(const FString& Filename, uint64 HashLo, uint64 HashHi, int64 Size, uint64 LUTVersion) const
{
    const FString Key = MakeKey(Filename, HashLo, HashHi, Size);
    FScopeLock Lock(&Mutex);
    const TUniquePtr<FEntry>* Found = Entries.Find(Key);
    if (!Found)
    {
        return false;
    }
    const FEntry* Entry = Found->Get();
    return Entry->LUTVersion == LUTVersion ||
           (Entry->LUTVersion == 0 && !HasBakedVertexColors(Entry->Meshes));
}

bool FJUSYNCMeshCache::Get(const FString& Filename, uint64 HashLo, uint64 HashHi, int64 Size, uint64 LUTVersion,
    TArray<FJUSYNCMeshData>& OutMeshes, TArray<FJUSYNCPointCloudData>& OutPointClouds) const
{
    const FString Key = MakeKey(Filename, HashLo, HashHi, Size);
    FScopeLock Lock(&Mutex);
    const TUniquePtr<FEntry>* Found = Entries.Find(Key);
    const FEntry* Entry = Found ? Found->Get() : nullptr;
    if (!Entry)
    {
        return false;
    }

    if (Entry->LUTVersion != LUTVersion &&
        !(Entry->LUTVersion == 0 && !HasBakedVertexColors(Entry->Meshes)))
    {
        return false;
    }

    OutMeshes = Entry->Meshes;
    OutPointClouds = Entry->PointClouds;
    // const Get still updates LRU by mutating the non-logical-const ordering arrays.
    const_cast<FJUSYNCMeshCache*>(this)->TouchLocked(Key);
    return true;
}

void FJUSYNCMeshCache::Store(const FString& Filename, uint64 HashLo, uint64 HashHi, int64 Size, uint64 LUTVersion,
    const TArray<FJUSYNCMeshData>& Meshes, const TArray<FJUSYNCPointCloudData>& PointClouds)
{
    if (Filename.IsEmpty())
    {
        return;
    }

    const FString Key = MakeKey(Filename, HashLo, HashHi, Size);
    FScopeLock Lock(&Mutex);

    if (Entries.Contains(Key))
    {
        RemoveLocked(Key);
    }

    TUniquePtr<FEntry> Entry = MakeUnique<FEntry>();
    Entry->Meshes = Meshes;
    Entry->PointClouds = PointClouds;
    Entry->EstimatedBytes = EstimatePayloadBytes(Entry->Meshes, Entry->PointClouds);
    Entry->LUTVersion = LUTVersion;

    TotalBytes += Entry->EstimatedBytes;
    Entries.Add(Key, MoveTemp(Entry));
    TouchLocked(Key);
    EvictLocked();
}

void FJUSYNCMeshCache::RemoveFile(const FString& Filename)
{
    FScopeLock Lock(&Mutex);
    TArray<FString> Keys;
    const FString Prefix = Filename + TEXT("|");
    for (const TPair<FString, TUniquePtr<FEntry>>& Pair : Entries)
    {
        if (Pair.Key.StartsWith(Prefix))
        {
            Keys.Add(Pair.Key);
        }
    }
    for (const FString& Key : Keys)
    {
        RemoveLocked(Key);
    }
}

void FJUSYNCMeshCache::Clear()
{
    FScopeLock Lock(&Mutex);
    Entries.Empty();
    LRUOrder.Empty();
    TotalBytes = 0;
}

void FJUSYNCMeshCache::SetLimits(int32 InMaxEntries, int64 InMaxBytes)
{
    FScopeLock Lock(&Mutex);
    MaxEntries = FMath::Max(1, InMaxEntries);
    MaxBytes = FMath::Max<int64>(1024 * 1024, InMaxBytes);
    EvictLocked();
}

int32 FJUSYNCMeshCache::GetEntryCount() const
{
    FScopeLock Lock(&Mutex);
    return Entries.Num();
}

int64 FJUSYNCMeshCache::GetEstimatedBytes() const
{
    FScopeLock Lock(&Mutex);
    return TotalBytes;
}

void FJUSYNCMeshCache::EvictLocked()
{
    while (Entries.Num() > MaxEntries || TotalBytes > MaxBytes)
    {
        FString VictimKey;
        for (const FString& Key : LRUOrder)
        {
            if (Entries.Contains(Key))
            {
                VictimKey = Key;
                break;
            }
        }
        if (VictimKey.IsEmpty())
        {
            break;
        }
        RemoveLocked(VictimKey);
    }
}

void FJUSYNCMeshCache::TouchLocked(const FString& Key)
{
    LRUOrder.Remove(Key);
    LRUOrder.Add(Key);
}

void FJUSYNCMeshCache::RemoveLocked(const FString& Key)
{
    if (TUniquePtr<FEntry>* Entry = Entries.Find(Key))
    {
        TotalBytes -= Entry->Get()->EstimatedBytes;
        Entries.Remove(Key);
    }
    LRUOrder.Remove(Key);
}
