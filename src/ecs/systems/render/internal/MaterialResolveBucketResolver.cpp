#include "MaterialResolveBucketResolver.h"
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/log/Log.h>

#include <unordered_map>

namespace hgl::ecs::internal
{
    void ResolveMIBuckets(
        const std::vector<ResolveTask> &tasks,
        const std::unordered_map<PrototypeKey, std::vector<size_t>, PrototypeKeyHash> &prototype_buckets,
        graph::MaterialRecipeRegistry *registry,
        std::vector<ResolvedMIBucket> &out_resolved_buckets,
        uint32_t &out_mi_bucket_count,
        uint32_t &inout_resolve_fail_count)
    {
        out_resolved_buckets.clear();
        out_mi_bucket_count = 0;

        if (!registry)
            return;

        for (const auto &[pkey, indices] : prototype_buckets)
        {
            if (indices.empty())
                continue;

            std::unordered_map<MIKey, std::vector<size_t>, MIKeyHash> mi_buckets;
            mi_buckets.reserve(indices.size());

            const uint64_t proto_hash = HashPrototypeKey(pkey);

            for (const size_t idx : indices)
            {
                const ResolveTask &task = tasks[idx];

                MIKey mikey;
                mikey.prototype_hash = proto_hash;
                mikey.instance_hash = task.instance_hash;
                mi_buckets[mikey].push_back(idx);
            }

            for (const auto &[mikey, mi_indices] : mi_buckets)
            {
                (void)mikey;
                ++out_mi_bucket_count;

                if (mi_indices.empty())
                    continue;

                const ResolveTask &seed = tasks[mi_indices.front()];
                const auto &seed_gvf = seed.geometry->GetGeometryVertexFormat();

                const graph::VIL *resolve_vil = nullptr;
                graph::MaterialBindingInstance *mi =
                    registry->ResolveOrCreateBindingInstance(seed.material_key,
                                                             *seed.recipe,
                                                             seed_gvf,
                                                             seed.slot->GetInstanceDataPtr(),
                                                             seed.slot->GetInstanceDataSize(),
                                                             nullptr,
                                                             &resolve_vil);

                GLogInfo("[ECS::MaterialResolveSystem] resolve_bucket key_hash=0x%llx recipe_prim=%u pipeline=%u mi=%p vil=%p bucket_size=%u",
                        static_cast<unsigned long long>(seed.material_key.Hash()),
                        static_cast<unsigned>(seed.recipe->prim),
                        static_cast<unsigned>(seed.recipe->pipeline),
                        mi,
                        resolve_vil,
                        static_cast<unsigned>(mi_indices.size()));

                if (!mi)
                {
                    inout_resolve_fail_count += static_cast<uint32_t>(mi_indices.size());
                    continue;
                }

                ResolvedMIBucket bucket;
                bucket.mi = mi;
                bucket.resolve_vil = resolve_vil;
                bucket.task_indices = mi_indices; 
                out_resolved_buckets.push_back(std::move(bucket));
            }
        }
    }
}
