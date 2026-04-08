#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
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
}//namespace mtl

using MaterialTemplateID    = int;
using MaterialInstanceID    = int;
using ShaderModuleMapByName = UnorderedMap<AnsiString,ShaderModule *>;

struct MaterialSpecKey
{
    AnsiString cache_name;
};

struct MaterialInstanceSpecKey
{
    MaterialTemplate *material = nullptr;
    const VIL *vil = nullptr;
    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;
    MaterialResourceDomain *domain = nullptr;
};

struct MaterialSpec
{
    enum class Family : uint8_t
    {
        Invalid = 0,
        Preset2D,
        Preset3D,
        Variant2D,
        Variant3D,
    };

    Family family = Family::Invalid;

    mtl::MaterialPreset preset{};
    const mtl::MaterialVariantKey *variant_key = nullptr;

    mtl::Material2DCreateConfig *cfg2d = nullptr;
    mtl::Material3DCreateConfig *cfg3d = nullptr;

    bool IsValid() const;
};

struct MaterialInstanceSpec
{
    MaterialTemplate *material = nullptr;
    MaterialResourceDomain *domain = nullptr;

    const VIL *vil = nullptr;
    const VILConfig *vil_cfg = nullptr;

    const void *instance_data = nullptr;
    uint32 instance_data_size = 0;

    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;

    bool IsValid() const { return material || domain; }
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

struct MaterialSlotAllocateStats
{
    uint64_t requests = 0;
    uint64_t created = 0;
    uint64_t with_mi = 0;
    uint64_t no_mi = 0;
    uint64_t failed = 0;
    uint64_t no_mi_payload_rejected = 0;
};

constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT = 20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

GRAPH_MODULE_CLASS(MaterialManager)
{
private:

    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    UnorderedMap<AnsiString,MaterialTemplate *> material_by_name;

    AutoIdObjectManager<MaterialTemplateID,            MaterialTemplate>          rm_shader_program;
    AutoIdObjectManager<MaterialInstanceID,         MaterialInstance>       rm_material_instance;

    std::unordered_map<MaterialTemplate *,          MaterialResourceDomain *>   default_domain_map;

    // Phase 3 — 域生命周期追踪：domain → 该域所有 DomainMaterialBinding
    std::unordered_map<MaterialResourceDomain *, std::vector<DomainMaterialBinding *>> domain_bindings_map;

    // Phase E — domain 句柄表（domain_id + generation，支持域整体替换）
    struct DomainEntry {
        MaterialResourceDomain *domain     = nullptr;
        uint32_t                generation = 0;  // 0 = invalid/released
    };
    std::vector<DomainEntry>                                    domain_table_;    // index 0 = invalid sentinel
    std::unordered_map<MaterialResourceDomain *, uint32_t>      domain_id_map_;   // reverse: ptr → id

    uint32_t RegisterDomain  (MaterialResourceDomain *domain);  ///< 首次注册返回新 id（1-based），重复注册返回已有 id
    void     UnregisterDomain(MaterialResourceDomain *domain);  ///< 标记条目失效（generation 保留，domain 置 null）

    std::atomic<uint64_t> acquire_material_requests {0};
    std::atomic<uint64_t> acquire_material_cache_lookups {0};
    std::atomic<uint64_t> acquire_material_cache_hits {0};
    std::atomic<uint64_t> acquire_material_cache_misses {0};
    std::atomic<uint64_t> acquire_material_created {0};
    std::atomic<uint64_t> acquire_fallback_used {0};

    std::atomic<uint64_t> acquire_mi_requests {0};
    std::atomic<uint64_t> acquire_mi_created {0};

    std::atomic<uint64_t> alloc_slot_requests {0};
    std::atomic<uint64_t> alloc_slot_created {0};
    std::atomic<uint64_t> alloc_slot_with_mi {0};
    std::atomic<uint64_t> alloc_slot_no_mi {0};
    std::atomic<uint64_t> alloc_slot_failed {0};
    std::atomic<uint64_t> alloc_slot_no_mi_payload_rejected {0};

    // Fallback material for error handling (initialized on first use)
    MaterialTemplate *fallback_material = nullptr;

private:

    MaterialManager(GraphicsContext *);
    ~MaterialManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    MaterialTemplate *CreateMaterial(const AnsiString &, const mtl::MaterialCreateInfo *);
    MaterialTemplate *CreateMaterial(const mtl::MaterialPreset, mtl::Material2DCreateConfig *);   ///<基于内置材质ID创建2D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialPreset, mtl::Material3DCreateConfig *);   ///<基于内置材质ID创建3D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *); ///<基于variant key创建2D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *); ///<基于variant key创建3D材质
    class GraphicsPipelineLayoutData *CreateMaterialGraphicsPipelineLayoutData(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager, const class GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(MaterialTemplate *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci);
    MaterialTemplate *TryGetCachedMaterial(const AnsiString &name);
    bool ExecuteMaterialBuildPipeline(MaterialTemplate *mtl,
                                      const AnsiString &mtl_name,
                                      const mtl::MaterialCreateInfo *mci,
                                      const ShaderStageMap &sci_map);

     MaterialTemplate *TryInitializeFallbackMaterial();
     MaterialTemplate *GetFallbackMaterial();

public: //Material resource access

    MaterialResourceDomain *GetOrCreateDefaultDomain(MaterialTemplate *mtl);

public: //Add

    MaterialTemplateID         Add(MaterialTemplate *          mtl ){return rm_shader_program.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}

public: //Get

    MaterialTemplate *         GetMaterial         (const MaterialTemplateID      &id){return rm_shader_program.Get(id);}
    MaterialInstance *      GetMaterialInstance (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}
    MaterialTemplate *         ResolveMaterial     (const MaterialInstance *   mi)const;

public: //Release

    void Release(MaterialTemplate *         mtl ){rm_shader_program.Release(mtl);}
    void Release(MaterialInstance * mi  )
    {
        rm_material_instance.Release(mi);
    }

    void Destroy(MaterialTemplate *mtl)
    {
        if (!mtl)
            return;

        auto dom_it = default_domain_map.find(mtl);
        if (dom_it != default_domain_map.end())
        {
            ReleaseMaterialResourceDomain(dom_it->second);
            default_domain_map.erase(dom_it);
        }

        const AnsiString &name = mtl->GetName();
        if (!name.IsEmpty())
            material_by_name.DeleteByKey(name);

        rm_shader_program.Release(mtl, true);
    }

    void Destroy(MaterialInstance *mi)
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
        const MaterialSlotAllocateStats slot_stats = GetMaterialSlotAllocateStats();

        if (mat_stats.requests > 0 || mi_stats.requests > 0 || slot_stats.requests > 0)
        {
            std::fprintf(stderr,
                "[MaterialManager] AcquireStats: material(req=%llu lookup=%llu hit=%llu miss=%llu created=%llu fallback=%llu) mi(req=%llu created=%llu) slot(req=%llu created=%llu with_mi=%llu no_mi=%llu failed=%llu no_mi_payload_rejected=%llu)\n",
                static_cast<unsigned long long>(mat_stats.requests),
                static_cast<unsigned long long>(mat_stats.cache_lookups),
                static_cast<unsigned long long>(mat_stats.cache_hits),
                static_cast<unsigned long long>(mat_stats.cache_misses),
                static_cast<unsigned long long>(mat_stats.created),
                static_cast<unsigned long long>(mat_stats.fallback_used),
                static_cast<unsigned long long>(mi_stats.requests),
                static_cast<unsigned long long>(mi_stats.created),
                static_cast<unsigned long long>(slot_stats.requests),
                static_cast<unsigned long long>(slot_stats.created),
                static_cast<unsigned long long>(slot_stats.with_mi),
                static_cast<unsigned long long>(slot_stats.no_mi),
                static_cast<unsigned long long>(slot_stats.failed),
                static_cast<unsigned long long>(slot_stats.no_mi_payload_rejected));
        }

        // 清理所有材质实例（MI dtor 会调 domain->FreeMISlot，domain 须在此之后释放）
        if (rm_material_instance.GetCount() > 0)
            rm_material_instance.Clear();

        // Phase 3: 清理所有 DomainMaterialBinding 及 MaterialResourceDomain
        for (auto &kv : domain_bindings_map)
        {
            for (auto *b : kv.second)
                delete b;
            delete kv.first;
        }
        domain_bindings_map.clear();

        // Phase 1: 清理懒初始化的 default domain
        for (auto &kv : default_domain_map)
            delete kv.second;
        default_domain_map.clear();

        // Phase E: 清理 domain 句柄表
        domain_table_.clear();
        domain_id_map_.clear();

        // 清理所有材质
        if (rm_shader_program.GetCount() > 0)
            rm_shader_program.Clear();

        if (material_by_name.GetCount() > 0)
            material_by_name.Clear();

        for (auto &stage_map : shader_module_by_name)
        {
            for (auto &kv : stage_map)
            {
                delete kv.second;
            }
            stage_map.Clear();
        }

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

    MaterialSlotAllocateStats GetMaterialSlotAllocateStats() const
    {
        MaterialSlotAllocateStats s;
        s.requests = alloc_slot_requests.load();
        s.created = alloc_slot_created.load();
        s.with_mi = alloc_slot_with_mi.load();
        s.no_mi = alloc_slot_no_mi.load();
        s.failed = alloc_slot_failed.load();
        s.no_mi_payload_rejected = alloc_slot_no_mi_payload_rejected.load();
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
        alloc_slot_requests.store(0);
        alloc_slot_created.store(0);
        alloc_slot_with_mi.store(0);
        alloc_slot_no_mi.store(0);
        alloc_slot_failed.store(0);
        alloc_slot_no_mi_payload_rejected.store(0);
    }

public: //MaterialTemplate

    MaterialTemplate *          AcquireMaterial (const MaterialSpec &spec, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialPreset, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialPreset, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);

public: //MaterialInstanceData

    /// @deprecated Use slot-first path via AllocMaterialInstanceSlot or MaterialAssetRegistry::ResolveMI().
    MaterialInstance *  AcquireMaterialInstance(const MaterialInstanceSpec &spec, MaterialInstanceSpecKey *out_key = nullptr);
    
    /// Phase A (NEW): Slot-first 接口 — 按 domain 分配实例槽并直接返回 slot。
    /// 调用方可直接传给 Primitive::BindMaterialSlot() 或缓存在组件中。
    /// @param domain 目标数据域（负责 MI 槽位与 MIT 布局）
    /// @param material MaterialTemplate（提供渲染管线、描述符定义）
    /// @param vil VertexInputLayout（来自 material->GetDefaultVIL() 或 config）
    /// @param preset GraphicsPipelinePreset（Solid3D/Solid2D/Wireframe 等）
    /// @param instance_data 可选初始 MI 数据
    /// @param instance_data_size MI 数据大小
    /// @return PrimitiveMaterialSlot（完整渲染绑定包）
    PrimitiveMaterialSlot AllocMaterialInstanceSlot(MaterialResourceDomain *domain,
                                                     MaterialTemplate *material,
                                                     const VIL *vil,
                                                     GraphicsPipelinePreset preset,
                                                     const void *instance_data = nullptr,
                                                     uint32_t instance_data_size = 0);
    
    bool                UpdateInstanceData(MaterialInstance *mi, const void *data, const uint32 data_size);

public: // MaterialResourceDomain — Phase 1 / Phase 3

    /**
     * 以语义级布局枚举创建资源域（推荐接口）。
     * layout 决定 MI 数据格式，tex_array_slots 声明本域提供的 TextureArray 集合（供方）。
     */
    MaterialResourceDomain *        CreateMaterialResourceDomain        (mtl::InstanceDataLayout layout,
                                                                         uint32_t max_count,
                                                                         uint8_t tex_array_slots = 0);

    /**
     * 以 MaterialTemplate 为模板创建资源域（过渡接口，Phase C 后废弃）。
     */
    MaterialResourceDomain *        CreateMaterialResourceDomain        (MaterialTemplate *mtl);

    /**
     * 创建一个 (domain, material) 绑定视图，并分配该 pair 专属的 VkDescriptorSet 集合。
     * Phase 2: 支持压缩绑定 Texture/Sampler/UBO/SSBO，与同一 Shader 的其它域完全隔离。
     * Phase 3: 同一 domain 可绑定多个 Material（Opaque + Masked 等），各 binding 独立管理。
     * 关系检查：MI stride 必须兼容；描述符集类型差异以 Warning 形式打印。
     */
    DomainMaterialBinding * CreateDomainMaterialBinding (MaterialResourceDomain *domain, MaterialTemplate *mtl);

    /**
     * 释放一个 DomainMaterialBinding，并将其从所属域的追踪列表中移除。
     * 注意：不释放关联的 MaterialResourceDomain。
     */
    void ReleaseDomainMaterialBinding(DomainMaterialBinding *binding);

    /**
     * 释放一个 MaterialResourceDomain 及其所有 DomainMaterialBinding。
     * 调用前请确保该域不再有存活的 MaterialInstance（否则 FreeMISlot 会访问已释放对象）。
     */
    void ReleaseMaterialResourceDomain(MaterialResourceDomain *domain);

    // Phase E — domain 句柄解析与热替换
    /**
     * 通过 domain_id + generation 安全查找 domain 指针。
     * generation 不匹配（域已被替换）或 id 越界时返回 nullptr。
     */
    MaterialResourceDomain *ResolveDomain(uint32_t domain_id, uint32_t generation) const;

    /**
     * 热替换指定槽位的 domain，并递增 generation 令旧 MI 句柄失效。
     * new_domain 可为 nullptr（仅失效不替换）。
     */
    void ReplaceDomain(uint32_t domain_id, MaterialResourceDomain *new_domain);

public: // MaterialResourceDomain MaterialInstanceData creation (Phase 1)

    // Create MI from a semantic-owned domain binding to a concrete runtime variant material.
    MaterialInstance *  CreateMaterialInstance(MaterialResourceDomain *domain,
                                               MaterialTemplate *material,
                                               const VIL *vil,
                                               const void *data,
                                               const uint32 data_size);

    // Phase D: update MI render material/vil without reallocating MI slot.
    bool RebindMaterialInstance(MaterialInstance *mi, MaterialTemplate *material, const VIL *vil);

public: // Phase 0 Stats — 帧级资源量观测

    /// 当前存活 MaterialTemplate 数量
    uint32_t GetMaterialCount()         const { return (uint32_t)rm_shader_program.GetCount(); }

    /// 当前存活 MaterialInstance 数量
    uint32_t GetMaterialInstanceCount() const { return (uint32_t)rm_material_instance.GetCount(); }

    /// 当前存活 MaterialResourceDomain 数量（Phase 3）
    uint32_t GetDomainCount()           const { return (uint32_t)domain_bindings_map.size(); }

    /// 当前总 DomainMaterialBinding 数量（Phase 3）
    uint32_t GetDomainBindingCount()    const
    {
        uint32_t n = 0;
        for (const auto &kv : domain_bindings_map)
            n += (uint32_t)kv.second.size();
        return n;
    }

};//class MaterialManager

}//namespace hgl::graph
