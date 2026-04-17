#pragma once

/// MaterialAssetRegistry.h — 从 MaterialRecipe 一站式创建材质层级
///
/// 内部按层级自动缓存：
///   ShaderMaterialProgram       — AcquireMaterial 已缓存
///   ResourceDomain — 按 domain_id 缓存
///   DMB            — 按 (material_name, domain_id, texture_config_hash) 缓存
///   MI             — per-object slot，每次新建

#include <hgl/graph/module/MaterialDomainHandle.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <string>
#include <unordered_map>
#include <cstdint>

namespace hgl::graph
{

class ShaderMaterialProgramManager;
class TextureManager;
class SamplerManager;
class MaterialInstance;

class MaterialAssetRegistry
{
    ShaderMaterialProgramManager *mm;
    TextureManager  *tm;
    SamplerManager  *sm;

    // (schema + domain_id) → ResourceDomain*
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

    MaterialAssetRegistry(ShaderMaterialProgramManager *mm, TextureManager *tm, SamplerManager *sm);
    ~MaterialAssetRegistry() = default;

    /// 核心 API：传入 record，返回三元组
    /// ShaderMaterialProgram 已缓存，Domain 按 domain_id 缓存，DMB 按纹理配置缓存
    MaterialDomainHandle Acquire(const mtl::MaterialRecipe &rec);

    /// 一站式：Acquire + CreateMI（推荐外部调用）
    /// - 大多数调用方只需要 MI，不需要直接接触 MaterialDomainHandle。
    /// - 若需要后续访问 DMB，可传 out_handle 取回完整句柄。
    MaterialInstance *AcquireMI(const mtl::MaterialRecipe &rec,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// 一站式：Acquire + 从 GVF 自动推算 VIL 覆写 + CreateMI
    /// - 对比 ShaderMaterialProgram DefaultVIL 与 Geometry 实际顶点格式，自动生成 VILConfig
    /// - 调用方无需手写 mi_vil_overrides
    MaterialInstance *AcquireMI(const mtl::MaterialRecipe &rec,
                                const GeometryVertexFormat &gvf,
                                const void *instance_data = nullptr,
                                uint32_t instance_data_size = 0,
                                MaterialDomainHandle *out_handle = nullptr);

    /// Record 驱动重载：pipeline 与可选 VIL 覆写均来自 rec
    MaterialInstance *CreateMI(const MaterialDomainHandle &handle,
                               const mtl::MaterialRecipe &rec,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);

    /// 统计
    uint32_t GetDomainCacheSize() const { return static_cast<uint32_t>(domain_cache.size()); }
    uint32_t GetDMBCacheSize()    const { return static_cast<uint32_t>(dmb_cache.size()); }
};

} // namespace hgl::graph
