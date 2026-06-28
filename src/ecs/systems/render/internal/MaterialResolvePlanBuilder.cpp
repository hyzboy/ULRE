#include "MaterialResolvePlanBuilder.h"
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/RecipeToKey.h>

namespace hgl::ecs::internal
{
    void BuildResolvePlan(
        const std::vector<std::shared_ptr<PrimitiveComponent>> &primitives,
        graph::mtl::MaterialRecipeStore *recipe_store,
        std::vector<ResolveTask> &out_tasks,
        std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> &out_prototype_buckets,
        uint32_t &out_resolve_input_count)
    {
        out_tasks.clear();
        out_prototype_buckets.clear();
        out_resolve_input_count = 0;

        out_tasks.reserve(primitives.size());
        out_prototype_buckets.reserve(primitives.size());

        for (auto &comp : primitives)
        {
            if (!comp || !comp->NeedsMaterialBindingResolve())
                continue;

            auto &slot = comp->GetMaterialResolveRequest();

            const graph::mtl::MaterialRecipe *recipe =
                recipe_store ? recipe_store->GetRecipe(slot.recipe_id) : nullptr;
            if (!recipe)
                continue;

            graph::Geometry *geom = comp->GetUnresolvedGeometry();
            if (!geom && comp->GetPrimitive())
                geom = comp->GetPrimitive()->GetGeometry();

            if (!geom)
                continue;

            const auto &gvf = geom->GetGeometryVertexFormat();
            ResolveTask task;
            task.comp = comp;
            task.slot = &slot;
            task.geometry = geom;
            task.recipe = recipe;
            task.material_key = graph::mtl::ResolveRecipePrimaryKey(*recipe);
            task.gvf_hash = HashGeometryVertexFormat(gvf);
            task.instance_hash = HashBytes(slot.GetInstanceDataPtr(), slot.GetInstanceDataSize());

            GLogInfo("[ECS::MaterialResolveSystem] resolve_input comp=%p recipe_id=%u preset=%u dim=%u recipe_prim=%u pipeline=%u geom=%p key_hash=0x%llx gvf_hash=0x%llx instance_hash=0x%llx",
                comp.get(),
                static_cast<unsigned>(slot.recipe_id),
                static_cast<unsigned>(recipe->preset),
                static_cast<unsigned>(recipe->dim),
                static_cast<unsigned>(recipe->prim),
                static_cast<unsigned>(recipe->pipeline),
                geom,
                static_cast<unsigned long long>(task.material_key.Hash()),
                static_cast<unsigned long long>(task.gvf_hash),
                static_cast<unsigned long long>(task.instance_hash));

            const size_t index = out_tasks.size();
            out_tasks.push_back(task);
            ++out_resolve_input_count;

            PrototypeKey pkey;
            pkey.material_key = task.material_key;
            pkey.gvf_hash = task.gvf_hash;
            out_prototype_buckets[pkey].push_back(index);
        }
    }
}
