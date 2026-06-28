#include <hgl/graph/module/MaterialResolveTieredCache.h>

namespace hgl::graph
{
    namespace
    {
        static size_t HashCombine(size_t seed, size_t value)
        {
            return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
        }
    }

    size_t ProgramCacheKeyHash::operator()(const ProgramCacheKey &k) const
    {
        size_t h = std::hash<uint64_t>{}(k.material_key.Hash());
        h = HashCombine(h, std::hash<uint64_t>{}(k.gvf_hash));
        h = HashCombine(h, std::hash<uint64_t>{}(k.feature_mask));
        h = HashCombine(h, std::hash<uint32_t>{}(k.rule_version));
        h = HashCombine(h, std::hash<uint64_t>{}(k.capability_mask));
        return h;
    }

    size_t PayloadCacheKeyHash::operator()(const PayloadCacheKey &k) const
    {
        size_t h = std::hash<uint32_t>{}(k.recipe_id);
        h = HashCombine(h, std::hash<uint64_t>{}(k.instance_data_hash));
        h = HashCombine(h, std::hash<uint32_t>{}(k.domain_id));
        h = HashCombine(h, std::hash<uint32_t>{}(k.runtime_texture_generation));
        return h;
    }

    size_t BindingCacheKeyHash::operator()(const BindingCacheKey &k) const
    {
        size_t h = std::hash<const ShaderMaterialProgram *>{}(k.program);
        h = HashCombine(h, std::hash<uint64_t>{}(k.payload_id));
        h = HashCombine(h, std::hash<uint32_t>{}(k.domain_id));
        h = HashCombine(h, std::hash<uint64_t>{}(k.layout_signature));
        return h;
    }

    ShaderMaterialProgram *MaterialResolveTieredCache::FindProgram(const ProgramCacheKey &key)
    {
        ++stats.program_requests;

        auto it = program_cache.find(key);
        if (it == program_cache.end())
        {
            ++stats.program_misses;
            return nullptr;
        }

        ++stats.program_hits;
        return it->second;
    }

    void MaterialResolveTieredCache::UpsertProgram(const ProgramCacheKey &key, ShaderMaterialProgram *program, bool created)
    {
        program_cache[key] = program;
        if (created)
            ++stats.program_creates;
    }

    MaterialInstancePayload *MaterialResolveTieredCache::FindPayload(const PayloadCacheKey &key)
    {
        ++stats.payload_requests;

        auto it = payload_cache.find(key);
        if (it == payload_cache.end())
        {
            ++stats.payload_misses;
            return nullptr;
        }

        ++stats.payload_hits;
        return it->second;
    }

    void MaterialResolveTieredCache::UpsertPayload(const PayloadCacheKey &key, MaterialInstancePayload *payload, bool created)
    {
        payload_cache[key] = payload;
        if (created)
            ++stats.payload_creates;
    }

    ProgramInstanceBinding *MaterialResolveTieredCache::FindBinding(const BindingCacheKey &key)
    {
        ++stats.binding_requests;

        auto it = binding_cache.find(key);
        if (it == binding_cache.end())
        {
            ++stats.binding_misses;
            return nullptr;
        }

        ++stats.binding_hits;
        return it->second;
    }

    void MaterialResolveTieredCache::UpsertBinding(const BindingCacheKey &key, ProgramInstanceBinding *binding, bool created)
    {
        binding_cache[key] = binding;
        if (created)
            ++stats.binding_creates;
    }

    void MaterialResolveTieredCache::Clear()
    {
        program_cache.clear();
        payload_cache.clear();
        binding_cache.clear();
    }
}
