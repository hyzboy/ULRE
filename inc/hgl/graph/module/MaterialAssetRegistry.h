#pragma once

/// MaterialAssetRegistry.h — 从 MaterialAssetRecord 一站式创建材质层级
///
/// 内部按层级自动缓存：
///   Material       — AcquireMaterial 已缓存
///   ResourceDomain — 按 domain_id 缓存
///   DMB            — 按 (material_name, domain_id, texture_config_hash) 缓存
///   MI             — per-object slot，每次新建

#include <hgl/graph/module/MaterialDomainHandle.h>
#include <hgl/graph/module/RuntimeMaterialRequest.h>
#include <hgl/mtl/MaterialAssetRecord.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <string>
#include <unordered_map>
#include <cstdint>

namespace hgl::graph
{

class MaterialManager;
class TextureManager;
class SamplerManager;
class MaterialInstance;

class MaterialAssetRegistry
{
    MaterialManager *mm;
    TextureManager  *tm;
    SamplerManager  *sm;

    // (material_cache_name + domain_id) → ResourceDomain*
    std::unordered_map<std::string, ResourceDomain*> domain_cache;

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

    // Semantic-only material registration cache:
    // key = semantic hash id (runtime policy fields excluded)
    std::unordered_map<SemanticMaterialId, mtl::MaterialAssetRecord> semantic_cache;

    struct FinalMaterialKey
    {
        SemanticMaterialId semantic_id = 0;
        RuntimeMaterialRequest request;
        GeometrySignature geometry;

        bool operator==(const FinalMaterialKey &o) const
        {
            return semantic_id == o.semantic_id
                && request == o.request
                && geometry == o.geometry;
        }
    };

    struct FinalMaterialKeyHash
    {
        size_t operator()(const FinalMaterialKey &k) const;
    };

    // Runtime resolved MI cache.
    std::unordered_map<FinalMaterialKey, MaterialInstance*, FinalMaterialKeyHash> final_mi_cache;

public:

    MaterialAssetRegistry(MaterialManager *mm, TextureManager *tm, SamplerManager *sm);
    ~MaterialAssetRegistry() = default;

    /// 核心 API：传入 record，返回三元组
    /// Material 已缓存，Domain 按 domain_id 缓存，DMB 按纹理配置缓存
    MaterialDomainHandle Acquire(const mtl::MaterialAssetRecord &rec);

    /// 注册最小语义材质并返回稳定ID（不创建Material/MI）。
    /// 注意：此ID故意不包含 pipeline/domain_id/mi_vil_overrides 等运行时策略字段。
    SemanticMaterialId RegisterSemanticMaterial(const mtl::MaterialAssetRecord &rec);

    /// 按语义ID查询原始语义记录。
    bool QuerySemanticMaterial(SemanticMaterialId id, mtl::MaterialAssetRecord &out_rec) const;

    /// 运行时解析入口：
    /// semantic_id + runtime request + geometry signature -> final MI
    ///
    /// 该路径为 Phase 2 新增，默认复用 Acquire/CreateMI 流程。
    MaterialInstance *ResolveMI(SemanticMaterialId semantic_id,
                                const RuntimeMaterialRequest &request,
                                const GeometrySignature &geometry,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// 一站式：Acquire + CreateMI（推荐外部调用）
    /// - 大多数调用方只需要 MI，不需要直接接触 MaterialDomainHandle。
    /// - 若需要后续访问 DMB，可传 out_handle 取回完整句柄。
    MaterialInstance *AcquireMI(const mtl::MaterialAssetRecord &rec,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// Record 驱动重载：pipeline 与可选 VIL 覆写均来自 rec
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               const mtl::MaterialAssetRecord &rec,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    /// 统计
    uint32_t GetDomainCacheSize() const { return static_cast<uint32_t>(domain_cache.size()); }
    uint32_t GetDMBCacheSize()    const { return static_cast<uint32_t>(dmb_cache.size()); }
    uint32_t GetSemanticMaterialCount() const { return static_cast<uint32_t>(semantic_cache.size()); }
    uint32_t GetResolvedMICacheSize() const { return static_cast<uint32_t>(final_mi_cache.size()); }
};

} // namespace hgl::graph
