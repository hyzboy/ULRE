#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKInstanceDataDomain.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
#include<hgl/graph/module/IDDManager.h>
#include<hgl/type/ObjectManager.h>
#include<hgl/graph/module/ShaderGenValidationTypes.h>
#include<ankerl/unordered_dense.h>
#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <cstdio>

namespace hgl::graph{

class ShaderCreateInfo;
class ShaderStageMap;
class MaterialAssetRegistry;

namespace mtl
{
    enum class MaterialPreset:uint8;
    struct MaterialVariantKey;
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;
}//namespace mtl

using MaterialTemplateID    = int;
using ShaderModuleMapByName = ankerl::unordered_dense::map<std::string,ShaderModule *>;

struct MaterialSpecKey
{
    std::string cache_name;
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

struct MaterialAcquireStats
{
    uint64_t requests = 0;
    uint64_t cache_lookups = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t created = 0;
    uint64_t fallback_used = 0;
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
    ankerl::unordered_dense::map<std::string,MaterialTemplate *> material_by_name;

    AutoIdObjectManager<MaterialTemplateID,            MaterialTemplate>          rm_shader_program;

    ankerl::unordered_dense::map<MaterialTemplate *,          IDDHandle>                  default_domain_map;

    // Phase 3 — 域生命周期追踪：domain → 该域所有 DomainMaterialBinding (P4: key changed to IDDHandle)
    ankerl::unordered_dense::map<IDDHandle, std::vector<DomainMaterialBinding *>> domain_bindings_map;

    std::atomic<uint64_t> acquire_material_requests {0};
    std::atomic<uint64_t> acquire_material_cache_lookups {0};
    std::atomic<uint64_t> acquire_material_cache_hits {0};
    std::atomic<uint64_t> acquire_material_cache_misses {0};
    std::atomic<uint64_t> acquire_material_created {0};
    std::atomic<uint64_t> acquire_fallback_used {0};

    std::atomic<uint64_t> alloc_slot_requests {0};
    std::atomic<uint64_t> alloc_slot_created {0};
    std::atomic<uint64_t> alloc_slot_with_mi {0};
    std::atomic<uint64_t> alloc_slot_no_mi {0};
    std::atomic<uint64_t> alloc_slot_failed {0};
    std::atomic<uint64_t> alloc_slot_no_mi_payload_rejected {0};

    // Fallback material for error handling (initialized on first use)
    MaterialTemplate *fallback_material = nullptr;

    // IDDManager — owns all InstanceDataDomain instances (P1+)
    IDDManager *idd_manager_ = nullptr;

private:

    MaterialManager(GraphicsContext *);
    ~MaterialManager()=default;

    friend class GraphModuleManager;

public:

    class MaterialAccessToken
    {
        friend class MaterialManager;
        friend class MaterialAssetRegistry;

    private:
        explicit MaterialAccessToken(int) {}

    public:
        MaterialAccessToken() = delete;
    };

private:

    static MaterialAccessToken MakeInternalAccessToken() { return MaterialAccessToken(0); }

private: // Helper methods with integrated DebugUtils

    MaterialTemplate *CreateMaterial(const std::string &, const mtl::MaterialCreateInfo *);
    MaterialTemplate *CreateMaterial(const mtl::MaterialPreset, mtl::Material2DCreateConfig *);   ///<基于内置材质ID创建2D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialPreset, mtl::Material3DCreateConfig *);   ///<基于内置材质ID创建3D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *); ///<基于variant key创建2D材质
    MaterialTemplate *CreateMaterial(const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *); ///<基于variant key创建3D材质
    class GraphicsPipelineLayoutData *CreateMaterialGraphicsPipelineLayoutData(const std::string &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const std::string &mtl_name, const class MaterialDescriptorManager *desc_manager, const class GraphicsPipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(MaterialTemplate *mtl, const std::string &mtl_name, const mtl::MaterialCreateInfo &mci);
    MaterialTemplate *TryGetCachedMaterial(const std::string &name);
    bool ExecuteMaterialBuildPipeline(MaterialTemplate *mtl,
                                      const std::string &mtl_name,
                                      const mtl::MaterialCreateInfo *mci,
                                      const ShaderStageMap &sci_map);

     MaterialTemplate *TryInitializeFallbackMaterial();
     MaterialTemplate *GetFallbackMaterial();

public: //Material resource access

    InstanceDataDomain *GetOrCreateDefaultDomain(MaterialTemplate *mtl);

    /// P5: expose IDDManager for callers that need handle↔pointer bridging.
    IDDManager *GetIDDManager() const { return idd_manager_; }

public: //Add

    MaterialTemplateID  Add(MaterialTemplate *  mtl ){return rm_shader_program.Add(mtl);}

public: //Get

    MaterialTemplate *  GetMaterial         (const MaterialTemplateID & id){return rm_shader_program.Get(id);}

public: //Release

    void Release(MaterialTemplate *         mtl ){rm_shader_program.Release(mtl);}

    void Destroy(MaterialTemplate *mtl)
    {
        if (!mtl)
            return;

        auto dom_it = default_domain_map.find(mtl);
        if (dom_it != default_domain_map.end())
        {
            ReleaseInstanceDataDomain(dom_it->second);
            default_domain_map.erase(dom_it);
        }

        const std::string &name = mtl->GetName();
        if (!name.empty())
            material_by_name.erase(name);

        rm_shader_program.Release(mtl, true);
    }

public: // Override Release from GraphModule - cleanup all resources

    void Release() override
    {
        const MaterialAcquireStats mat_stats = GetMaterialAcquireStats();
        const MaterialSlotAllocateStats slot_stats = GetMaterialSlotAllocateStats();

        if (mat_stats.requests > 0 || slot_stats.requests > 0)
        {
            std::fprintf(stderr,
                "[MaterialManager] AcquireStats: material(req=%llu lookup=%llu hit=%llu miss=%llu created=%llu fallback=%llu) slot(req=%llu created=%llu with_mi=%llu no_mi=%llu failed=%llu no_mi_payload_rejected=%llu)\n",
                static_cast<unsigned long long>(mat_stats.requests),
                static_cast<unsigned long long>(mat_stats.cache_lookups),
                static_cast<unsigned long long>(mat_stats.cache_hits),
                static_cast<unsigned long long>(mat_stats.cache_misses),
                static_cast<unsigned long long>(mat_stats.created),
                static_cast<unsigned long long>(mat_stats.fallback_used),
                static_cast<unsigned long long>(slot_stats.requests),
                static_cast<unsigned long long>(slot_stats.created),
                static_cast<unsigned long long>(slot_stats.with_mi),
                static_cast<unsigned long long>(slot_stats.no_mi),
                static_cast<unsigned long long>(slot_stats.failed),
                static_cast<unsigned long long>(slot_stats.no_mi_payload_rejected));
        }

        // Phase 3: 清理所有 DomainMaterialBinding 及 InstanceDataDomain
        for (auto &kv : domain_bindings_map)
        {
            for (auto *b : kv.second)
                delete b;
            // P3: domain lifetime owned by idd_manager_; do NOT delete kv.first here
        }
        domain_bindings_map.clear();

        // Phase 1: 清理懒初始化的 default domain
        // P3: domains owned by idd_manager_; do NOT delete here
        default_domain_map.clear();

        // 清理所有材质
        if (rm_shader_program.GetCount() > 0)
            rm_shader_program.Clear();

        if (!material_by_name.empty())
            material_by_name.clear();

        for (auto &stage_map : shader_module_by_name)
        {
            for (auto &kv : stage_map)
            {
                delete kv.second;
            }
            stage_map.clear();
        }

        ResetAcquireStats();

        // P1: teardown IDDManager after all domains are cleaned up
        delete idd_manager_;
        idd_manager_ = nullptr;
    }

public: //Shader

    const ShaderModule *CreateShaderModule(const std::string &shader_module_name, const ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const std::string &shader_module_name,
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
        alloc_slot_requests.store(0);
        alloc_slot_created.store(0);
        alloc_slot_with_mi.store(0);
        alloc_slot_no_mi.store(0);
        alloc_slot_failed.store(0);
        alloc_slot_no_mi_payload_rejected.store(0);
    }

public: //MaterialTemplate

    MaterialTemplate *          AcquireMaterial (const MaterialSpec &spec, MaterialSpecKey *out_key, MaterialAccessToken);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialPreset, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key, MaterialAccessToken);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialPreset, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key, MaterialAccessToken);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key, MaterialAccessToken);
    MaterialTemplate *          AcquireMaterial (const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key, MaterialAccessToken);

    // Internal bridge for engine modules: bypass deprecated public wrappers,
    // still funneled through tokenized implementation.
    MaterialTemplate *          AcquireMaterialInternal(const MaterialSpec &spec, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterialInternal(const mtl::MaterialPreset, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterialInternal(const mtl::MaterialPreset, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterialInternal(const mtl::MaterialVariantKey &, mtl::Material2DCreateConfig *, MaterialSpecKey *out_key = nullptr);
    MaterialTemplate *          AcquireMaterialInternal(const mtl::MaterialVariantKey &, mtl::Material3DCreateConfig *, MaterialSpecKey *out_key = nullptr);

public: //MaterialInstanceData

    /// Phase A (NEW): Slot-first 接口 — 按 domain 分配实例槽并直接返回 slot。
    /// 调用方可直接传给 Primitive::BindMaterialSlot() 或缓存在组件中。
    /// 注意：返回的 slot 仅含 domain 与 mi_id；调用方需自行填写
    /// material_template、vil、preset 和 texture_array_slot_flags。
    /// @param domain 目标数据域（负责 MI 槽位与布局）
    /// @param instance_data 可选初始 MI 数据
    /// @param instance_data_size MI 数据大小
    /// @return PrimitiveMaterialSlot（domain + mi_id 已填写）
    PrimitiveMaterialSlot AllocMaterialInstanceSlot(InstanceDataDomain *domain,
                                                     const void *instance_data = nullptr,
                                                     uint32_t instance_data_size = 0);

    /// P5: IDDHandle overload — resolves handle to domain ptr internally.
    PrimitiveMaterialSlot AllocMaterialInstanceSlot(IDDHandle domain_handle,
                                                     const void *instance_data = nullptr,
                                                     uint32_t instance_data_size = 0);
    
public: // InstanceDataDomain — Phase 1 / Phase 3

    /**
     * 以语义级布局枚举创建资源域（推荐接口）。
     * layout 决定 MI 数据格式，tex_array_slots 声明本域提供的 TextureArray 集合（供方）。
     */
    InstanceDataDomain *        CreateInstanceDataDomain        (mtl::InstanceDataLayout layout,
                                                                         uint32_t max_count,
                                                                         uint8_t tex_array_slots = 0);

    /**
     * 以 MaterialTemplate 为模板创建资源域（过渡接口，Phase C 后废弃）。
     */
    InstanceDataDomain *        CreateInstanceDataDomain        (MaterialTemplate *mtl);

    /**
     * 创建一个 (domain, material) 绑定视图，并分配该 pair 专属的 VkDescriptorSet 集合。
     * Phase 2: 支持压缩绑定 Texture/Sampler/UBO/SSBO，与同一 Shader 的其它域完全隔离。
     * Phase 3: 同一 domain 可绑定多个 Material（Opaque + Masked 等），各 binding 独立管理。
     * 关系检查：MI stride 必须兼容；描述符集类型差异以 Warning 形式打印。
     */
    DomainMaterialBinding * CreateDomainMaterialBinding (InstanceDataDomain *domain, MaterialTemplate *mtl);

    /**
     * 按 (domain handle, material) 查询已创建的 DomainMaterialBinding。
     * 返回空表示该 pair 尚未创建绑定视图。
     */
    DomainMaterialBinding * FindDomainMaterialBinding   (IDDHandle handle, MaterialTemplate *mtl) const;

    /**
     * 释放一个 DomainMaterialBinding，并将其从所属域的追踪列表中移除。
     * 注意：不释放关联的 InstanceDataDomain。
     */
    void ReleaseDomainMaterialBinding(DomainMaterialBinding *binding);

    /**
     * 释放一个 InstanceDataDomain 及其所有 DomainMaterialBinding。
     * 调用前请确保该域不再有存活的 MaterialInstance（否则 FreeMISlot 会访问已释放对象）。
     */
    void ReleaseInstanceDataDomain(InstanceDataDomain *domain);
    void ReleaseInstanceDataDomain(IDDHandle handle);

public: // Phase 0 Stats — 帧级资源量观测

    /// 当前存活 MaterialTemplate 数量
    uint32_t GetMaterialCount()         const { return (uint32_t)rm_shader_program.GetCount(); }

    /// 当前存活 InstanceDataDomain 数量（Phase 3）
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
