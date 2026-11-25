#pragma once

#include<hgl/type/String.h>
#include<hgl/mtl/DebugViewMode.h>

namespace hgl::graph
{
    // Thread-safe bridge state; owner is an opaque identity, never dereferenced.
    class DebugViewState
    {
    public:
        static bool RequestMode(mtl::DebugViewMode);
        static mtl::DebugViewMode GetRequestedMode();
        static mtl::DebugViewMode GetAppliedMode();
        static uint64_t GetTargetID();
        static uint32_t GetTrackedCount();
        static AnsiString GetStatusJSON();
        static void SetStatus(const void *owner,mtl::DebugViewMode,uint64_t target_id,
                              uint32_t batches,const char *error,uint32_t entities=0);
        static void ClearStatus(const void *owner,bool reset_requested);
    };
}
