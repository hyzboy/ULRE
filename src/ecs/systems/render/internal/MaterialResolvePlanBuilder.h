#pragma once

#include "MaterialResolveInternals.h"

#include <hgl/mtl/MaterialRecipeStore.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace hgl::ecs::internal
{
    void BuildResolvePlan(
        const std::vector<std::shared_ptr<PrimitiveComponent>> &primitives,
        graph::mtl::MaterialRecipeStore *recipe_store,
        std::vector<ResolveTask> &out_tasks,
        std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> &out_prototype_buckets,
        uint32_t &out_resolve_input_count);
}
