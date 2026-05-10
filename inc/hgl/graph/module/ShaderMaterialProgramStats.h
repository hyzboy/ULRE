#pragma once

#include <hgl/mtl/ShaderProgramKey.h>
#include <hgl/mtl/MaterialKey.h>
#include <hgl/type/String.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace hgl::graph
{
    class ShaderMaterialProgram;

    struct MaterialAcquireStats
    {
        uint64_t requests = 0;
        uint64_t cache_lookups = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        uint64_t created = 0;
        uint64_t fallback_used = 0;
    };

    struct MaterialInstanceAcquireStats
    {
        uint64_t requests = 0;
        uint64_t created = 0;
    };

    struct MaterialKeyAxisMismatchStats
    {
        uint64_t total = 0;
        uint64_t def_id = 0;
        uint64_t schema = 0;
        uint64_t glsl_version = 0;
        uint64_t vk_version = 0;
        uint64_t spv_version = 0;
    };

    struct ShaderProgramKeyCoverageStats
    {
        uint64_t vertex_seen = 0;
        uint64_t vertex_unique = 0;
        uint64_t fragment_seen = 0;
        uint64_t fragment_unique = 0;
    };

    struct ShaderProgramKeyShadowCacheStats
    {
        uint64_t vertex_hits = 0;
        uint64_t vertex_misses = 0;
        uint64_t fragment_hits = 0;
        uint64_t fragment_misses = 0;
        uint64_t combined_hits = 0;
        uint64_t combined_misses = 0;
        uint64_t vertex_entries = 0;
        uint64_t fragment_entries = 0;
        uint64_t combined_pointer_match_hits = 0;
        uint64_t combined_pointer_mismatch_hits = 0;
    };

    class ShaderMaterialProgramStats
    {
    public:
        enum : uint32_t
        {
            MATERIAL_KEY_MISMATCH_DEF_ID       = 1u << 0,
            MATERIAL_KEY_MISMATCH_SCHEMA       = 1u << 1,
            MATERIAL_KEY_MISMATCH_GLSL_VERSION = 1u << 2,
            MATERIAL_KEY_MISMATCH_VK_VERSION   = 1u << 3,
            MATERIAL_KEY_MISMATCH_SPV_VERSION  = 1u << 4,
        };

    private:
        std::atomic<uint64_t> acquire_material_requests {0};
        std::atomic<uint64_t> acquire_material_cache_lookups {0};
        std::atomic<uint64_t> acquire_material_cache_hits {0};
        std::atomic<uint64_t> acquire_material_cache_misses {0};
        std::atomic<uint64_t> acquire_material_created {0};
        std::atomic<uint64_t> acquire_fallback_used {0};

        std::atomic<uint64_t> acquire_mi_requests {0};
        std::atomic<uint64_t> acquire_mi_created {0};

        std::atomic<uint64_t> by_key_hits {0};

        std::atomic<uint64_t> key_axis_mismatch_total {0};
        std::atomic<uint64_t> key_axis_mismatch_def_id {0};
        std::atomic<uint64_t> key_axis_mismatch_schema {0};
        std::atomic<uint64_t> key_axis_mismatch_glsl {0};
        std::atomic<uint64_t> key_axis_mismatch_vk {0};
        std::atomic<uint64_t> key_axis_mismatch_spv {0};

        std::atomic<uint64_t> vertex_program_key_seen {0};
        std::atomic<uint64_t> fragment_program_key_seen {0};
        std::unordered_set<uint64_t> vertex_program_key_unique_hashes;
        std::unordered_set<uint64_t> fragment_program_key_unique_hashes;

        std::unordered_map<uint64_t, ShaderMaterialProgram *> vertex_program_shadow_cache;
        std::unordered_map<uint64_t, ShaderMaterialProgram *> fragment_program_shadow_cache;
        std::atomic<uint64_t> shadow_vertex_hits {0};
        std::atomic<uint64_t> shadow_vertex_misses {0};
        std::atomic<uint64_t> shadow_fragment_hits {0};
        std::atomic<uint64_t> shadow_fragment_misses {0};
        std::atomic<uint64_t> shadow_combined_hits {0};
        std::atomic<uint64_t> shadow_combined_misses {0};
        std::atomic<uint64_t> shadow_combined_ptr_match_hits {0};
        std::atomic<uint64_t> shadow_combined_ptr_mismatch_hits {0};

        mutable std::mutex mutex;

    public:
        void RecordMaterialRequest() noexcept { acquire_material_requests.fetch_add(1, std::memory_order_relaxed); }
        void RecordMaterialCreated() noexcept { acquire_material_created.fetch_add(1, std::memory_order_relaxed); }
        void RecordFallbackUsed() noexcept { acquire_fallback_used.fetch_add(1, std::memory_order_relaxed); }
        void RecordMaterialInstanceRequest() noexcept { acquire_mi_requests.fetch_add(1, std::memory_order_relaxed); }
        void RecordMaterialInstanceCreated() noexcept { acquire_mi_created.fetch_add(1, std::memory_order_relaxed); }
        void RecordByKeyHit() noexcept { by_key_hits.fetch_add(1, std::memory_order_relaxed); }

        void RecordMaterialKeyAxisMismatch(uint32_t mismatch_mask) noexcept;
        void RecordShaderProgramKeyCoverage(const mtl::VertexProgramKey &vkey,
                                            const mtl::FragmentProgramKey &fkey);
        void RecordShaderProgramShadowCacheLookup(const mtl::VertexProgramKey &vkey,
                                                  const mtl::FragmentProgramKey &fkey);
        void RecordShaderProgramShadowCacheInsert(const mtl::VertexProgramKey &vkey,
                                                  const mtl::FragmentProgramKey &fkey,
                                                  ShaderMaterialProgram *program);

        MaterialAcquireStats GetMaterialAcquireStats() const noexcept;
        MaterialInstanceAcquireStats GetMaterialInstanceAcquireStats() const noexcept;
        MaterialKeyAxisMismatchStats GetMaterialKeyAxisMismatchStats() const noexcept;
        ShaderProgramKeyCoverageStats GetShaderProgramKeyCoverageStats() const;
        ShaderProgramKeyShadowCacheStats GetShaderProgramKeyShadowCacheStats() const;

        uint64_t GetByKeyHits() const noexcept { return by_key_hits.load(std::memory_order_relaxed); }

        void Reset();
        void DumpKeyMapDiagnostics(size_t material_by_key_size) const;

        void LogProgramKeyTriplet(const char *phase,
                                  const mtl::MaterialKey &material_key,
                                  const PrimitiveType primitive,
                                  const bool has_local_to_world) const noexcept;

        void LogEffectiveFeatureMaskConsistency(const ShaderMaterialProgram *prog,
                                                const mtl::MaterialKey &request_key,
                                                const char *phase) const;

        void LogMaterialKeyAxisMismatchDetails(const mtl::MaterialKey &cached_key,
                                               const mtl::MaterialKey &request_key,
                                               const uint32_t mismatch_mask) const;

        void LogGetOrCreateProgramByKeyRequest(const uint64_t key_hash,
                                               const uint32_t recipe_prim,
                                               const uint32_t recipe_preset,
                                               const uint32_t recipe_pipeline) const;

        void LogGetOrCreateProgramByKeyCacheHit(const ShaderMaterialProgram *program) const;
        void LogGetOrCreateProgramByKeyCreated(const ShaderMaterialProgram *program) const;
        void LogGetOrCreateProgramByKeyAliasWarning(const uint64_t key_hash) const;

        void LogCreateMaterialFromRecordRequest(const uint32_t preset,
                                                const uint32_t dim,
                                                const uint32_t prim,
                                                const bool l2w,
                                                const uint32_t pipeline,
                                                const uint64_t key_hash) const;

        void LogCreateMaterialFromRecordBillboard(const int preset,
                                                  const int use_texture_array,
                                                  const int blend_mode) const;

        void LogCreateMaterialFromRecord2D(const uint32_t prim,
                                           const uint32_t preset) const;

        void LogCreateMaterialFromRecord3D(const uint32_t prim,
                                           const uint32_t preset,
                                           const bool include_camera,
                                           const bool include_sky) const;

        void LogMaterialFinalizeSummary(const AnsiString &material_name,
                                        const uint32_t mi_data_bytes,
                                        const uint32_t mi_max_count,
                                        const uint32_t schema,
                                        const std::string &schema_file,
                                        const size_t descriptor_set_count) const;

        void LogFallbackMaterialInitialized(const AnsiString &material_name) const;
        void LogBillboardDomainArrayKey(const uint64_t cache_key_hash) const;
        void LogCreateMaterialKey3DProfileNull(const uint64_t key_hash) const;
        void LogCreateMaterialKey3DCreateInfoFailed(const mtl::MaterialVariantKey &key,
                                                    const std::string &cfg_hash) const;

        void LogLine(const std::string &line) const;
    };
}
