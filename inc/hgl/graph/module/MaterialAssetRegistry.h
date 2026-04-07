#pragma once

/// MaterialAssetRegistry.h — 从 MaterialAssetRecord 一站式创建材质层级
///
/// 内部按层级自动缓存：
///   MaterialTemplate       — AcquireMaterial 已缓存
///   MaterialResourceDomain — 按 domain_id 缓存
///   DMB            — 按 (material_name, domain_id, texture_config_hash) 缓存
///   MI             — per-object slot，每次新建

#include <hgl/graph/module/MaterialDomainHandle.h>
#include <hgl/graph/module/RuntimeMaterialRequest.h>
#include <hgl/mtl/MaterialAssetRecord.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <hgl/graph/PrimitiveMaterialSlot.h>
#include <hgl/log/Log.h>

#include <string>
#include <unordered_map>
#include <cstdint>

namespace hgl::graph
{

class MaterialManager;
class TextureManager;
class SamplerManager;
class MaterialInstance;
class MaterialTemplate;

class MaterialAssetRegistry
{
    OBJECT_LOGGER

private:

    MaterialManager *mm;
    TextureManager  *tm;
    SamplerManager  *sm;

    // (material_cache_name + domain_id) → MaterialResourceDomain*
    std::unordered_map<std::string, MaterialResourceDomain*> domain_cache;

    // DMB 缓存 key = (material_cache_name, domain_id, texture_config_hash)
    struct DMBKey
    {
        std::string material_name;
        std::string domain_id;
        uint64_t    texture_hash;

        bool operator==(const DMBKey &o) const
        {
            return material_name == o.material_name
                && domain_id     == o.domain_id
                && texture_hash  == o.texture_hash;
        }
    };

    struct DMBKeyHash
    {
        size_t operator()(const DMBKey &k) const;
    };

    std::unordered_map<DMBKey, DomainMaterialBinding*, DMBKeyHash> dmb_cache;

    struct SemanticMaterialEntry
    {
        mtl::MaterialAssetRecord rec;

        // Phase B groundwork: semantic-level canonical material/domain.
        // Behavior remains compatible until ResolveMI switches to semantic-owned domain path.
        MaterialTemplate *canonical_material = nullptr;
        MaterialResourceDomain *shared_domain = nullptr;
    };

    // Semantic-only material registration cache:
    // key = semantic hash id (runtime policy fields excluded)
    std::unordered_map<SemanticMaterialId, SemanticMaterialEntry> semantic_cache;

    struct VariantKey
    {
        SemanticMaterialId semantic_id = 0;
        RuntimeMaterialRequest request;
        GeometrySignature geometry;

        bool operator==(const VariantKey &o) const
        {
            return semantic_id == o.semantic_id
                && request == o.request
                && geometry == o.geometry;
        }
    };

    struct VariantKeyHash
    {
        size_t operator()(const VariantKey &k) const;
    };

    // Runtime variant cache (Phase C): final runtime key -> concrete material variant.
    std::unordered_map<VariantKey, MaterialTemplate*, VariantKeyHash> variant_cache;

    struct EntitySemanticKey
    {
        uint64_t entity_id = 0;
        SemanticMaterialId semantic_id = 0;

        bool operator==(const EntitySemanticKey &o) const
        {
            return entity_id == o.entity_id && semantic_id == o.semantic_id;
        }
    };

    struct EntitySemanticKeyHash
    {
        size_t operator()(const EntitySemanticKey &k) const;
    };

    // Runtime MI cache (Phase D): entity + semantic -> stable MI slot.
    std::unordered_map<EntitySemanticKey, MaterialInstance*, EntitySemanticKeyHash> entity_mi_cache;

    // Legacy path: unresolved callers without entity id still use variant-key-level MI cache.
    std::unordered_map<VariantKey, MaterialInstance*, VariantKeyHash> legacy_final_mi_cache;

    // Resolve path observability counters (Phase D rollout).
    uint64_t legacy_resolve_hit_count = 0;
    uint64_t legacy_resolve_miss_count = 0;
    uint64_t entity_resolve_hit_count = 0;
    uint64_t entity_resolve_miss_count = 0;
    bool legacy_resolve_warned = false;

public:

    MaterialAssetRegistry(MaterialManager *mm, TextureManager *tm, SamplerManager *sm);
    ~MaterialAssetRegistry() = default;

    /// 核心 API：传入 record，返回三元组
    /// MaterialTemplate 已缓存，Domain 按 domain_id 缓存，DMB 按纹理配置缓存
    MaterialDomainHandle Acquire(const mtl::MaterialAssetRecord &rec);

    /// 注册最小语义材质并返回稳定ID（不创建Material/MI）。
    /// 注意：此ID故意不包含 pipeline/domain_id/mi_vil_overrides 等运行时策略字段。
    SemanticMaterialId RegisterSemanticMaterial(const mtl::MaterialAssetRecord &rec);

    /// 按语义ID查询原始语义记录。
    bool QuerySemanticMaterial(SemanticMaterialId id, mtl::MaterialAssetRecord &out_rec) const;

    /// 运行时解析入口：
    /// semantic_id + runtime request + geometry signature -> final PrimitiveMaterialSlot
    ///
    /// 返回的槽位指向 entity_mi_cache 内部管理的域槽，生命周期归 Registry 所有。
    /// 调用方可直接传给 Primitive::BindMaterialSlot()。
    PrimitiveMaterialSlot ResolveMI(SemanticMaterialId semantic_id,
                                const RuntimeMaterialRequest &request,
                                const GeometrySignature &geometry,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    // Phase D path: resolve with entity id to guarantee per-entity stable MI slot.
    PrimitiveMaterialSlot ResolveMI(uint64_t entity_id,
                                SemanticMaterialId semantic_id,
                                const RuntimeMaterialRequest &request,
                                const GeometrySignature &geometry,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// 一站式：Acquire + CreateMI
    /// @deprecated 新代码请使用 ResolveMI(entity_id, semantic_id, ...) per-entity 路径。
    ///   AcquireMI 不携带 entity_id，无法稳定分配 per-entity MI 槽位；仅供工具 / 离线路径使用。
    MaterialInstance *AcquireMI(const mtl::MaterialAssetRecord &rec,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// Record 驱动重载：pipeline 与可选 VIL 覆写均来自 rec
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               const mtl::MaterialAssetRecord &rec,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    // Phase D lifecycle helpers for entity-scoped MI cache.
    void ReleaseEntityResolvedMI(uint64_t entity_id, SemanticMaterialId semantic_id = 0);

    /// 统计
    uint32_t GetDomainCacheSize() const { return static_cast<uint32_t>(domain_cache.size()); }
    uint32_t GetDMBCacheSize()    const { return static_cast<uint32_t>(dmb_cache.size()); }
    uint32_t GetSemanticMaterialCount() const { return static_cast<uint32_t>(semantic_cache.size()); }
    uint32_t GetResolvedMICacheSize() const { return static_cast<uint32_t>(legacy_final_mi_cache.size() + entity_mi_cache.size()); }
    uint64_t GetLegacyResolveHitCount() const { return legacy_resolve_hit_count; }
    uint64_t GetLegacyResolveMissCount() const { return legacy_resolve_miss_count; }
    uint64_t GetEntityResolveHitCount() const { return entity_resolve_hit_count; }
    uint64_t GetEntityResolveMissCount() const { return entity_resolve_miss_count; }
};

} // namespace hgl::graph
