#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace hgl
{
    namespace graph
    {
        class Primitive;
    }

    namespace ecs
    {
        // Two-level material variant cache for ECS collect path.
        //
        // Level 1 (global): variant_hash (semantic_id + request + geometry traits)
        //   → "resolved" flag, shared across ALL entities / Primitives with the same
        //   material variant.  A hash present here means domain/material/VIL are valid.
        //
        // Level 2 (per-Primitive): Primitive* → last-bound variant_hash + geometry hash.
        //   Lets the collect system skip BindMaterialSlot when the Primitive already has
        //   the correct variant bound and its geometry layout has not changed.
        //
        // The cache does NOT store MaterialDomainHandle / PrimitiveMaterialSlot payloads;
        // those are owned by MaterialAssetRegistry's entity_mi_cache.
        class MaterialCache
        {
        private:

            // Level 1: globally known-valid variant hashes (shared across all Primitives).
            std::unordered_set<uint64_t> resolved_variants;

            // Level 2: per-Primitive binding state.
            struct PrimitiveBindingState
            {
                uint64_t bound_variant_hash   = 0;
                uint32_t geometry_layout_hash = 0;
                bool     valid                = false;
            };
            std::unordered_map<const graph::Primitive *, PrimitiveBindingState> primitive_binding;

            size_t frame_l1_hit              = 0;
            size_t frame_l1_miss             = 0;
            size_t frame_l2_bind_skip        = 0;
            size_t frame_geometry_invalidate = 0;

        public:

            void BeginFrame()
            {
                frame_l1_hit              = 0;
                frame_l1_miss             = 0;
                frame_l2_bind_skip        = 0;
                frame_geometry_invalidate = 0;
            }

            // Level-1 probe: returns true if variant_hash is already known-valid.
            // Shared result — reflects any Primitive that previously resolved this variant.
            bool ProbeVariant(uint64_t variant_hash)
            {
                if (resolved_variants.count(variant_hash))
                {
                    ++frame_l1_hit;
                    return true;
                }
                ++frame_l1_miss;
                return false;
            }

            void MarkVariantResolved(uint64_t variant_hash)
            {
                resolved_variants.insert(variant_hash);
            }

            // Level-2 probe: returns true if this Primitive already has variant_hash bound
            // with the given geometry_layout_hash.  Marks the state invalid if geometry changed.
            bool ProbePrimitiveBinding(const graph::Primitive *primitive,
                                       uint64_t variant_hash,
                                       uint32_t geometry_layout_hash)
            {
                if (!primitive)
                    return false;

                auto it = primitive_binding.find(primitive);
                if (it == primitive_binding.end())
                    return false;

                auto &state = it->second;
                if (!state.valid)
                    return false;

                if (state.geometry_layout_hash != geometry_layout_hash)
                {
                    state.valid = false;
                    ++frame_geometry_invalidate;
                    return false;
                }

                if (state.bound_variant_hash != variant_hash)
                    return false;

                ++frame_l2_bind_skip;
                return true;
            }

            void MarkPrimitiveBound(const graph::Primitive *primitive,
                                    uint64_t variant_hash,
                                    uint32_t geometry_layout_hash)
            {
                if (!primitive)
                    return;

                auto &state              = primitive_binding[primitive];
                state.bound_variant_hash   = variant_hash;
                state.geometry_layout_hash = geometry_layout_hash;
                state.valid                = true;
            }

            void ErasePrimitiveBinding(const graph::Primitive *primitive)
            {
                auto it = primitive_binding.find(primitive);
                if (it != primitive_binding.end())
                    it->second.valid = false;
            }

            size_t GetFrameL1HitCount()             const { return frame_l1_hit; }
            size_t GetFrameL1MissCount()            const { return frame_l1_miss; }
            size_t GetFrameL2BindSkipCount()        const { return frame_l2_bind_skip; }
            size_t GetFrameGeometryInvalidateCount() const { return frame_geometry_invalidate; }
        };
    }
}
