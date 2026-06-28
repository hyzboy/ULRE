#pragma once

#include <hgl/graph/module/MaterialDecoupledTypes.h>
#include <hgl/mtl/MaterialKey.h>

#include <cstdint>
#include <unordered_map>

namespace hgl::graph
{
    class ShaderMaterialProgram;
    class ResourceDomain;

    struct ProgramCacheKey
    {
        mtl::MaterialKey material_key{};
        uint64_t gvf_hash = 0;
        uint64_t feature_mask = 0;
        uint32_t rule_version = 0;
        uint64_t capability_mask = 0;

        bool operator==(const ProgramCacheKey &rhs) const
        {
            return material_key == rhs.material_key
                && gvf_hash == rhs.gvf_hash
                && feature_mask == rhs.feature_mask
                && rule_version == rhs.rule_version
                && capability_mask == rhs.capability_mask;
        }
    };

    struct ProgramCacheKeyHash
    {
        size_t operator()(const ProgramCacheKey &k) const;
    };

    struct PayloadCacheKey
    {
        uint32_t recipe_id = 0xFFFFFFFFu;
        uint64_t instance_data_hash = 0;
        uint32_t domain_id = 0xFFFFFFFFu;
        uint32_t runtime_texture_generation = 0;

        bool operator==(const PayloadCacheKey &rhs) const
        {
            return recipe_id == rhs.recipe_id
                && instance_data_hash == rhs.instance_data_hash
                && domain_id == rhs.domain_id
                && runtime_texture_generation == rhs.runtime_texture_generation;
        }
    };

    struct PayloadCacheKeyHash
    {
        size_t operator()(const PayloadCacheKey &k) const;
    };

    struct BindingCacheKey
    {
        const ShaderMaterialProgram *program = nullptr;
        MaterialPayloadID payload_id = 0;
        uint32_t domain_id = 0xFFFFFFFFu;
        uint64_t layout_signature = 0;

        bool operator==(const BindingCacheKey &rhs) const
        {
            return program == rhs.program
                && payload_id == rhs.payload_id
                && domain_id == rhs.domain_id
                && layout_signature == rhs.layout_signature;
        }
    };

    struct BindingCacheKeyHash
    {
        size_t operator()(const BindingCacheKey &k) const;
    };

    struct MaterialResolveTieredCacheStats
    {
        uint64_t program_requests = 0;
        uint64_t program_hits = 0;
        uint64_t program_misses = 0;
        uint64_t program_creates = 0;

        uint64_t payload_requests = 0;
        uint64_t payload_hits = 0;
        uint64_t payload_misses = 0;
        uint64_t payload_creates = 0;

        uint64_t binding_requests = 0;
        uint64_t binding_hits = 0;
        uint64_t binding_misses = 0;
        uint64_t binding_creates = 0;
    };

    class MaterialResolveTieredCache
    {
    public:
        ShaderMaterialProgram *FindProgram(const ProgramCacheKey &key);
        void UpsertProgram(const ProgramCacheKey &key, ShaderMaterialProgram *program, bool created);

        MaterialInstancePayload *FindPayload(const PayloadCacheKey &key);
        void UpsertPayload(const PayloadCacheKey &key, MaterialInstancePayload *payload, bool created);

        ProgramInstanceBinding *FindBinding(const BindingCacheKey &key);
        void UpsertBinding(const BindingCacheKey &key, ProgramInstanceBinding *binding, bool created);

        void Clear();

        const MaterialResolveTieredCacheStats &GetStats() const { return stats; }
        void ResetStats() { stats = MaterialResolveTieredCacheStats{}; }

    private:
        std::unordered_map<ProgramCacheKey, ShaderMaterialProgram *, ProgramCacheKeyHash> program_cache;
        std::unordered_map<PayloadCacheKey, MaterialInstancePayload *, PayloadCacheKeyHash> payload_cache;
        std::unordered_map<BindingCacheKey, ProgramInstanceBinding *, BindingCacheKeyHash> binding_cache;
        MaterialResolveTieredCacheStats stats{};
    };
}
