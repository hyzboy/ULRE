#pragma once

#include "MaterialResolveBucketResolver.h"

#include <hgl/graph/module/PrimitiveManager.h>

namespace hgl::ecs::internal
{
    struct ApplyStageStats
    {
        uint32_t resolved_count = 0;
        uint32_t primitive_created_count = 0;
        uint32_t primitive_updated_count = 0;
        uint32_t primitive_recreated_count = 0;
    };

    void ApplyResolvedMIBuckets(
        std::vector<ResolveTask> &tasks,
        const std::vector<ResolvedMIBucket> &resolved_buckets,
        graph::PrimitiveManager *prim_mgr,
        ApplyStageStats &out_stats,
        uint32_t &inout_resolve_fail_count);
}
