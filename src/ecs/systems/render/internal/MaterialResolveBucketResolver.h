#pragma once

#include "MaterialResolveInternals.h"

#include <hgl/graph/module/MaterialRecipeRegistry.h>

#include <memory>

#include <unordered_map>
#include <vector>

namespace hgl::ecs::internal
{
    struct ResolvedMIBucket
    {
        graph::MaterialBindingInstance *mi = nullptr;
        const graph::VIL *resolve_vil = nullptr;
        std::unique_ptr<graph::MaterialInstancePayload> resolved_payload;
        std::unique_ptr<graph::ProgramInstanceBinding> resolved_program_binding;
        std::vector<size_t> task_indices;
    };

    void ResolveMIBuckets(
        const std::vector<ResolveTask> &tasks,
        const std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> &prototype_buckets,
        graph::MaterialRecipeRegistry *registry,
        std::vector<ResolvedMIBucket> &out_resolved_buckets,
        uint32_t &out_mi_bucket_count,
        uint32_t &inout_resolve_fail_count);
}
