#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKResourceDomain.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ObjectManager.h>
#include<hgl/graph/module/ShaderGenValidationTypes.h>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <cstdio>

namespace hgl::graph{

class ShaderCreateInfo;
class ShaderStageMap;

namespace mtl
{
    enum class MaterialPreset:uint8;
    struct MaterialVariantKey;
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;
    struct MaterialRecipe;
}//namespace mtl

using MaterialID            = int;
using MaterialBindingInstanceID    = int;
using ShaderModuleMapByName = UnorderedMap<AnsiString,ShaderModule *>;

struct MaterialSpecKey
{
    AnsiString cache_name;
};

struct MaterialInstanceSpecKey
{
    ShaderMaterialProgram *material = nullptr;
    const VIL *vil = nullptr;
    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;
    ResourceDomain *domain = nullptr;
};

struct MaterialInstanceSpec
{
    ShaderMaterialProgram *material = nullptr;
    ResourceDomain *domain = nullptr;

    const VIL *vil = nullptr;
    const VILConfig *vil_cfg = nullptr;

    const void *instance_data = nullptr;
    uint32 instance_data_size = 0;

    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;

    bool IsValid() const { return material != nullptr && domain != nullptr; }
};

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

constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT = 20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

GRAPH_MODULE_CLASS(ShaderMaterialProgramManager)
{
private:

    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];

    // Step 3: MaterialKey → Program* (alias map; ownership stays in rm_material)
    std::unordered_map<mtl::MaterialKey, ShaderMaterialProgram *> material_by_key;

    AutoIdObjectManager<MaterialID,             ShaderMaterialProgram>           rm_material;                ///<材质合集
    AutoIdObjectManager<MaterialBindingInstanceID,     MaterialBindingInstance>   rm_material_instance;       ///<材质实例合集

    // Phase 3 — 域生命周期追踪：domain → 该域所有 DomainResourceBinding
    std::unordered_map<ResourceDomain *, std::vector<DomainResourceBinding *>> domain_bindings_map;

    std::atomic<uint64_t> acquire_material_requests {0};
    std::atomic<uint64_t> acquire_material_cache_lookups {0};
    std::atomic<uint64_t> acquire_material_cache_hits {0};
    std::atomic<uint64_t> acquire_material_cache_misses {0};
    std::atomic<uint64_t> acquire_material_created {0};
    std::atomic<uint64_t> acquire_fallback_used {0};

    std::atomic<uint64_t> acquire_mi_requests {0};
    std::atomic<uint64_t> acquire_mi_created {0};

    // Step 3: material_by_key diagnostics
    std::atomic<uint64_t> by_key_hits             {0};

    // Fallback material for error handling (initialized on first use)
    ShaderMaterialProgram *fallback_material = nullptr;

private:

    ShaderMaterialProgramManager(GraphicsContext *);
    ~ShaderMaterialProgramManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    ShaderMaterialProgram *CreateMaterial(const AnsiString &, const mtl::MaterialCreateInfo *);
    ShaderMaterialProgram *CreateMaterial(const mtl::MaterialPreset, mtl::Material2DCreateConfig *);   ///<基于内置材质ID创建2D材质
    ShaderMaterialProgram *CreateMaterial(const mtl::MaterialPreset, mtl::Material3DCreateConfig *);   ///<基于内置材质ID创建3D材质
    ShaderMaterialProgram *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *); ///<基于variant key创建2D材质
    ShaderMaterialProgram *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *); ///<基于variant key创建3D材质
    class GraphicsPipelineLayoutData *CreateMaterialGraphicsPipelineLayoutData(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager, const class GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(ShaderMaterialProgram *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci);
    bool ExecuteMaterialBuildPipeline(ShaderMaterialProgram *mtl,
                                      const AnsiString &mtl_name,
                                      const mtl::MaterialCreateInfo *mci,
                                      const ShaderStageMap &sci_map);

    ShaderMaterialProgram *ResolveCreateFailureWithFallback(ShaderMaterialProgram *created);

     ShaderMaterialProgram *TryInitializeFallbackMaterial();
     ShaderMaterialProgram *GetFallbackMaterial();

public: //Add

    MaterialID              Add(ShaderMaterialProgram *          mtl ){return rm_material.Add(mtl);}
    MaterialBindingInstanceID      Add(MaterialBindingInstance *  mi  ){return rm_material_instance.Add(mi);}

public: //Get

    ShaderMaterialProgram *          GetShaderMaterialProgram         (const MaterialID           &id){return rm_material.Get(id);}
    MaterialBindingInstance *  GetResolvedBindingInstance(const MaterialBindingInstanceID   &id){return rm_material_instance.Get(id);}

public: //Release

    void Release(ShaderMaterialProgram *         mtl ){rm_material.Release(mtl);}
    void Release(MaterialBindingInstance * mi  ){rm_material_instance.Release(mi);}

    void Destroy(ShaderMaterialProgram *mtl)
    {
        if (!mtl)
            return;

        if (mtl->HasMaterialKey())
            material_by_key.erase(mtl->GetMaterialKey());

        rm_material.Release(mtl, true);
    }

    void Destroy(MaterialBindingInstance *mi)
    {
        if (!mi)
            return;

        rm_material_instance.Release(mi, true);
    }

public: // Override Release from GraphModule - cleanup all resources

    void Release() override
    {
        const MaterialAcquireStats mat_stats = GetMaterialAcquireStats();
        const MaterialInstanceAcquireStats mi_stats = GetMaterialInstanceAcquireStats();

        if (mat_stats.requests > 0 || mi_stats.requests > 0)
        {
            std::fprintf(stderr,
                "[ShaderMaterialProgramManager] AcquireStats: material(req=%llu lookup=%llu hit=%llu miss=%llu created=%llu fallback=%llu) mi(req=%llu created=%llu)\n",
                static_cast<unsigned long long>(mat_stats.requests),
                static_cast<unsigned long long>(mat_stats.cache_lookups),
                static_cast<unsigned long long>(mat_stats.cache_hits),
                static_cast<unsigned long long>(mat_stats.cache_misses),
                static_cast<unsigned long long>(mat_stats.created),
                static_cast<unsigned long long>(mat_stats.fallback_used),
                static_cast<unsigned long long>(mi_stats.requests),
                static_cast<unsigned long long>(mi_stats.created));
        }

        // 清理所有材质实例（MI dtor 会调 domain->FreeMISlot，domain 须在此之后释放）
        if (rm_material_instance.GetCount() > 0)
            rm_material_instance.Clear();

        // Phase 3: 只清理 binding；domain 生命周期由 ResourceDomainManager 接管。
        for (auto &kv : domain_bindings_map)
        {
            for (auto *b : kv.second)
                delete b;
        }
        domain_bindings_map.clear();

        // 清理所有材质
        if (rm_material.GetCount() > 0)
            rm_material.Clear();

        material_by_key.clear();

        for (auto &stage_map : shader_module_by_name)
        {
            for (auto &kv : stage_map)
            {
                delete kv.second;
            }
            stage_map.Clear();
        }

        fallback_material = nullptr;

        ResetAcquireStats();
    }

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const AnsiString &shader_module_name,
                                                  const VkShaderStageFlagBits stage,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);

public: //ShaderGen Profiler (debug entry, collect-only)

    void ResetShaderGenProfiler();

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshot() const;

    bool GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name = nullptr) const;

    std::vector<ShaderGenValidationReportRecord> GetShaderGenRecentValidationReports(const uint32_t max_count = 64) const;

    std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count = 128) const;

public: // Acquire stats

    MaterialAcquireStats GetMaterialAcquireStats() const
    {
        MaterialAcquireStats s;
        s.requests = acquire_material_requests.load();
        s.cache_lookups = acquire_material_cache_lookups.load();
        s.cache_hits = acquire_material_cache_hits.load();
        s.cache_misses = acquire_material_cache_misses.load();
        s.created = acquire_material_created.load();
            s.fallback_used = acquire_fallback_used.load();
        return s;
    }

    MaterialInstanceAcquireStats GetMaterialInstanceAcquireStats() const
    {
        MaterialInstanceAcquireStats s;
        s.requests = acquire_mi_requests.load();
        s.created = acquire_mi_created.load();
        return s;
    }

    void ResetAcquireStats()
    {
        acquire_material_requests.store(0);
        acquire_material_cache_lookups.store(0);
        acquire_material_cache_hits.store(0);
        acquire_material_cache_misses.store(0);
        acquire_material_created.store(0);
            acquire_fallback_used.store(0);
        acquire_mi_requests.store(0);
        acquire_mi_created.store(0);
        by_key_hits.store(0);
    }

    void DumpKeyMapDiagnostics() const;

public: //ShaderMaterialProgram

    /// Step 5: MaterialKey direct-path lookup; falls back to recipe-based creation on miss.
    /// Checks material_by_key first; on miss calls CreateMaterialFromRecord which populates the cache.
    ShaderMaterialProgram *GetOrCreateProgramByKey(const mtl::MaterialKey &key,
                                                   const mtl::MaterialRecipe &recipe);

    ShaderMaterialProgram *          ResolveOrCreateProgram (const mtl::MaterialPreset, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    ShaderMaterialProgram *          ResolveOrCreateProgram (const mtl::MaterialPreset, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    ShaderMaterialProgram *          ResolveOrCreateProgram (const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    ShaderMaterialProgram *          ResolveOrCreateProgram (const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);

public: //MaterialBindingInstanceData

    MaterialBindingInstance *  AcquireMaterialInstance(const MaterialInstanceSpec &spec, MaterialInstanceSpecKey *out_key = nullptr);
    bool                UpdateInstanceData(MaterialBindingInstance *mi, const void *data, const uint32 data_size);

public: // ResourceDomain — Phase 1 / Phase 3

    /**
     * 创建一个 (domain, material) 绑定视图，并分配该 pair 专属的 VkDescriptorSet 集合。
     * Phase 2: 支持压缩绑定 Texture/Sampler/UBO/SSBO，与同一 Shader 的其它域完全隔离。
     * Phase 3: 同一 domain 可绑定多个 Material（Opaque + Masked 等），各 binding 独立管理。
     * 关系检查：MI stride 必须兼容；描述符集类型差异以 Warning 形式打印。
     */
    DomainResourceBinding * CreateDomainMaterialBinding (ResourceDomain *domain, ShaderMaterialProgram *mtl);

    /**
     * 释放一个 DomainResourceBinding，并将其从所属域的追踪列表中移除。
     * 注意：不释放关联的 ResourceDomain。
     */
    void ReleaseDomainMaterialBinding(DomainResourceBinding *binding);

    /// 查找已有 (domain, material) 绑定，不存在则返回 nullptr。
    DomainResourceBinding *FindDomainMaterialBinding(ResourceDomain *domain, ShaderMaterialProgram *mtl) const;

public: // ResourceDomain MaterialBindingInstanceData creation (Phase 1)

    /// 从资源域分配 MI，走域独立的数据池（旧 ShaderMaterialProgram 池不变）。
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VIL *vil = nullptr);
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VILConfig *vil_cfg);
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VIL *vil, const void *data, const uint32 data_size);
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VILConfig *vil_cfg, const void *data, const uint32 data_size);

    template<typename T>
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VIL *vil, const T *data)
    {
        return CreateMaterialInstance(material, domain, vil, data, sizeof(T));
    }

    template<typename T>
    MaterialBindingInstance *  CreateMaterialInstance(ShaderMaterialProgram *material, ResourceDomain *domain, const VILConfig *vil_cfg, const T *data)
    {
        return CreateMaterialInstance(material, domain, vil_cfg, data, sizeof(T));
    }

public: // Phase 0 Stats — 帧级资源量观测

    /// 当前存活 ShaderMaterialProgram 数量
    uint32_t GetMaterialCount()         const { return (uint32_t)rm_material.GetCount(); }

    /// 当前存活 MaterialBindingInstance 数量
    uint32_t GetMaterialInstanceCount() const { return (uint32_t)rm_material_instance.GetCount(); }

    /// 当前存活 ResourceDomain 数量（Phase 3）
    uint32_t GetDomainCount()           const { return (uint32_t)domain_bindings_map.size(); }

    /// 当前总 DomainResourceBinding 数量（Phase 3）
    uint32_t GetDomainBindingCount()    const
    {
        uint32_t n = 0;
        for (const auto &kv : domain_bindings_map)
            n += (uint32_t)kv.second.size();
        return n;
    }

};//class ShaderMaterialProgramManager

}//namespace hgl::graph
