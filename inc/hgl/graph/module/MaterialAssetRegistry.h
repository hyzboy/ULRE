#pragma once

/// MaterialAssetRegistry.h — 从 MaterialAssetRecord 一站式创建材质层级
///
/// 内部按层级自动缓存：
///   Material       — AcquireMaterial 已缓存
///   ResourceDomain — 按 domain_id 缓存
///   DMB            — 按 (material_name, domain_id, texture_config_hash) 缓存
///   MI             — per-object slot，每次新建

#include <hgl/graph/module/MaterialDomainHandle.h>
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
class VertexInputLayout;
class VILConfig;

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

public:

    MaterialAssetRegistry(MaterialManager *mm, TextureManager *tm, SamplerManager *sm);
    ~MaterialAssetRegistry() = default;

    /// 核心 API：传入 record，返回三元组
    /// Material 已缓存，Domain 按 domain_id 缓存，DMB 按纹理配置缓存
    MaterialDomainHandle Acquire(const mtl::MaterialAssetRecord &rec);

    /// 从三元组创建 MI（per-object slot，每次都新建，用于合批）
    [[deprecated("Use CreateMI(handle, MaterialAssetRecord, ...) so MI vertex-input overrides are carried by record")]]
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               GraphicsPipelinePreset pipeline,
                               const VertexInputLayout *vil = nullptr,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    /// VILConfig 重载
    [[deprecated("Use CreateMI(handle, MaterialAssetRecord, ...) so MI vertex-input overrides are carried by record")]]
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               GraphicsPipelinePreset pipeline,
                               const VILConfig *vil_cfg,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    /// Record 驱动重载：pipeline 与可选 VIL 覆写均来自 rec
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               const mtl::MaterialAssetRecord &rec,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    /// 统计
    uint32_t GetDomainCacheSize() const { return static_cast<uint32_t>(domain_cache.size()); }
    uint32_t GetDMBCacheSize()    const { return static_cast<uint32_t>(dmb_cache.size()); }
};

} // namespace hgl::graph
