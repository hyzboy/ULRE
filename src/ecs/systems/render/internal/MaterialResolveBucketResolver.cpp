#include "MaterialResolveBucketResolver.h"
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include <hgl/log/Log.h>

#include <atomic>
#include <unordered_map>

namespace hgl::ecs::internal
{
    static graph::MaterialPayloadID AllocateShadowPayloadID()
    {
        static std::atomic<uint64_t> next_id{1};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    static graph::ProgramBindingID AllocateShadowBindingID()
    {
        static std::atomic<uint64_t> next_id{1};
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

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

                const uint32_t domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);
                const auto *seed_slot = seed.slot;

                bucket.resolved_payload = std::make_unique<graph::MaterialInstancePayload>();
                bucket.resolved_payload->id = AllocateShadowPayloadID();
                bucket.resolved_payload->schema_version = 0;
                bucket.resolved_payload->domain_id = domain_id;
                bucket.resolved_payload->instance_hash = seed.instance_hash;
                if (seed_slot && seed_slot->GetInstanceDataPtr() && seed_slot->GetInstanceDataSize() > 0)
                {
                    const auto *data = static_cast<const uint8_t *>(seed_slot->GetInstanceDataPtr());
                    bucket.resolved_payload->bytes.assign(data, data + seed_slot->GetInstanceDataSize());
                }

                bucket.resolved_program_binding = std::make_unique<graph::ProgramInstanceBinding>();
                bucket.resolved_program_binding->id = AllocateShadowBindingID();
                bucket.resolved_program_binding->program = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
                bucket.resolved_program_binding->payload = bucket.resolved_payload.get();
                bucket.resolved_program_binding->domain = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
                bucket.resolved_program_binding->layout_signature = 0;
                bucket.resolved_program_binding->legacy_binding_instance = mi;
                bucket.resolved_program_binding->legacy_vil = resolve_vil;

                bucket.task_indices = mi_indices; 
                out_resolved_buckets.push_back(std::move(bucket));
            }
        }
    }
}
