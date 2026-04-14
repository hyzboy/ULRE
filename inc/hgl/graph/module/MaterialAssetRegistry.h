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
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <hgl/graph/PrimitiveMaterialSlot.h>
#include <hgl/log/Log.h>

#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>

namespace hgl::graph
{
class MaterialManager;
class TextureManager;
class SamplerManager;
class MaterialTemplate;
class Geometry;

using MaterialInstanceHandle = uint64_t;
constexpr MaterialInstanceHandle InvalidMaterialInstanceHandle = 0;

enum class MaterialRebindCopyPolicy : uint8_t
{
    None = 0,
    CompatiblePrefix,
    LayoutAware,
};

struct MaterialBindingInit
{
    MaterialTemplate *material = nullptr;
    MRDHandle         domain_handle;
    const VIL *vil = nullptr;
    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;
    mtl::MaterialPreset material_preset = mtl::MaterialPreset::Standard;
    const void *instance_data = nullptr;
    uint32_t instance_data_size = 0;
    const uint32_t *mit_data = nullptr;
    uint32_t mit_data_count = 0;
};

struct MaterialBindingRebind
{
    MaterialTemplate *new_material = nullptr;
    MRDHandle         new_domain_handle;
    const VIL *new_vil = nullptr;
    GraphicsPipelinePreset new_preset = GraphicsPipelinePreset::Solid3D;
    mtl::MaterialPreset new_material_preset = mtl::MaterialPreset::Standard;
    MaterialRebindCopyPolicy copy_policy = MaterialRebindCopyPolicy::CompatiblePrefix;
};

class MaterialAssetRegistry
{
    OBJECT_LOGGER

private:

    MaterialManager *mm;
    TextureManager  *tm;
    SamplerManager  *sm;

    // (material_cache_name + domain_id) → MRDHandle
    std::unordered_map<std::string, MRDHandle> domain_cache;

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
        // canonical_material / shared_domain removed:
        // MaterialTemplate is resolved lazily at render time by ResolveMI(),
        // which combines semantic + Geometry VAB + runtime state via Acquire().
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

    // Runtime variant cache (Phase C): final runtime key -> resolved handle.
    // Read-before-write: hit skips QuerySemanticMaterial + Acquire() entirely.
    std::unordered_map<VariantKey, MaterialDomainHandle, VariantKeyHash> variant_cache;
    uint64_t variant_cache_hit_count  = 0;
    uint64_t variant_cache_miss_count = 0;

    // Phase D key: (entity, semantic, request_variant, geometry_variant).
    // Allows one entity to hold multiple concurrent variant slots for the same semantic.
    struct EntityVariantKey
    {
        uint64_t entity_id    = 0;
        SemanticMaterialId semantic_id = 0;
        uint64_t request_hash = 0;   // FNV-1a of RuntimeMaterialRequest fields
        uint64_t geometry_hash = 0;  // geometry_layout_hash | (vil_hash << 32)

        bool operator==(const EntityVariantKey &o) const
        {
            return entity_id    == o.entity_id
                && semantic_id  == o.semantic_id
                && request_hash == o.request_hash
                && geometry_hash == o.geometry_hash;
        }
    };

    struct EntityVariantKeyHash
    {
        size_t operator()(const EntityVariantKey &k) const;
    };

    // Runtime slot cache (Phase D): entity + semantic variant -> stable MI slot.
    std::unordered_map<EntityVariantKey, PrimitiveMaterialSlot, EntityVariantKeyHash> entity_mi_cache;

    // Legacy path: unresolved callers without entity id still use variant-key-level slot cache.
    std::unordered_map<VariantKey, PrimitiveMaterialSlot, VariantKeyHash> legacy_final_mi_cache;

    // Resolve path observability counters (Phase D rollout).
    uint64_t legacy_resolve_hit_count = 0;
    uint64_t legacy_resolve_miss_count = 0;
    uint64_t entity_resolve_hit_count = 0;
    uint64_t entity_resolve_miss_count = 0;
    bool legacy_resolve_warned = false;

    struct MaterialBindingRecord
    {
        MaterialInstanceHandle handle = InvalidMaterialInstanceHandle;
        MaterialTemplate *material_template = nullptr;
        MRDHandle domain_handle;
        int mi_id = -1;
        const VIL *vil = nullptr;
        GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;
        mtl::MaterialPreset material_preset = mtl::MaterialPreset::Standard;
        uint8_t texture_array_slot_flags = 0;
        std::vector<uint32_t> instance_payload;
        std::vector<uint32_t> mit_packed;
        std::vector<int8_t> mit_slot_offset;
        uint32_t binding_version = 1;
        bool alive = true;
    };

    std::unordered_map<MaterialInstanceHandle, MaterialBindingRecord> handle_table;
    MaterialInstanceHandle next_handle = InvalidMaterialInstanceHandle + 1;
    uint64_t handle_rebind_count = 0;
    uint64_t cross_domain_rebind_count = 0;
    uint64_t handle_rebind_fail_count = 0;

public:

    MaterialAssetRegistry(MaterialManager *mm, TextureManager *tm, SamplerManager *sm);
    ~MaterialAssetRegistry() = default;

    /// 核心 API：传入 record，返回三元组
    /// MaterialTemplate 已缓存，Domain 按 domain_id 缓存，DMB 按纹理配置缓存
    MaterialDomainHandle Acquire(const mtl::MaterialAssetRecord &rec);

    /// Resolve the VIL that should be used by geometry creation for a record.
    /// If geometry is provided, formats are derived from geometry VAB; otherwise
    /// material default VIL fallback is used.
    const VIL *ResolveVIL(const MaterialTemplate *material,
                          const mtl::MaterialAssetRecord &rec,
                          const Geometry *geometry = nullptr) const;

    /// 注册最小语义材质并返回稳定ID（不创建Material/MI）。
    /// 注意：此ID故意不包含 pipeline/domain_id 等运行时策略字段。
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

    // Phase D lifecycle helpers for entity-scoped MI cache.
    void ReleaseEntityResolvedMI(uint64_t entity_id, SemanticMaterialId semantic_id = 0);

    // Stage 1 - handle-first API skeleton.
    MaterialInstanceHandle AllocateHandle(const MaterialBindingInit &init);
    bool BuildSlot(MaterialInstanceHandle handle, PrimitiveMaterialSlot &out_slot) const;
    bool RebindHandle(MaterialInstanceHandle handle, const MaterialBindingRebind &req);
    bool WriteMIData(MaterialInstanceHandle handle, const void *data, uint32_t size);
    bool SetTextureArrayLayer(MaterialInstanceHandle handle, mtl::SamplerSlot slot, uint32_t layer);
    bool ReleaseHandle(MaterialInstanceHandle handle);

    // Stage 1 - observability for handle path.
    bool QueryBindingVersion(MaterialInstanceHandle handle, uint32_t &out_version) const;
    bool QueryHandleAlive(MaterialInstanceHandle handle) const;
    uint64_t GetCrossDomainRebindCount() const { return cross_domain_rebind_count; }
    uint64_t GetHandleRebindFailCount() const { return handle_rebind_fail_count; }

    /// 统计
    uint32_t GetDomainCacheSize() const { return static_cast<uint32_t>(domain_cache.size()); }
    uint32_t GetDMBCacheSize()    const { return static_cast<uint32_t>(dmb_cache.size()); }
    uint32_t GetSemanticMaterialCount() const { return static_cast<uint32_t>(semantic_cache.size()); }
    uint32_t GetResolvedMICacheSize() const { return static_cast<uint32_t>(legacy_final_mi_cache.size() + entity_mi_cache.size()); }
    uint64_t GetVariantCacheHitCount()  const { return variant_cache_hit_count; }
    uint64_t GetVariantCacheMissCount() const { return variant_cache_miss_count; }
    uint64_t GetLegacyResolveHitCount() const { return legacy_resolve_hit_count; }
    uint64_t GetLegacyResolveMissCount() const { return legacy_resolve_miss_count; }
    uint64_t GetEntityResolveHitCount() const { return entity_resolve_hit_count; }
    uint64_t GetEntityResolveMissCount() const { return entity_resolve_miss_count; }
};

} // namespace hgl::graph
