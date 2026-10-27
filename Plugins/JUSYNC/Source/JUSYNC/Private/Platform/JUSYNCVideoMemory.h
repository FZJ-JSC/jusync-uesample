// Queries the per-process video memory budget, the same number the RHI's
// OOM handling and UE's VRAM-aware systems use on Windows
// (IDXGIAdapter3::QueryVideoMemoryInfo). "Free" here means headroom within
// THIS process's local budget (Budget - CurrentUsage), which is the correct
// ceiling for caching more GPU resources - not "total minus everyone's".

#pragma once

#include "CoreMinimal.h"

namespace JUSYNCVideoMemory
{
    /** True on success; free/budget are bytes of the LOCAL segment. */
    bool QueryProcessLocalBudget(int64& OutFreeBytes, int64& OutBudgetBytes);
}
