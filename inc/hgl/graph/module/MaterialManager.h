#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ObjectManager.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/module/RendererShaderGenAdapter.h>

namespace hgl::graph{

namespace mtl
{
    enum class MaterialPreset:uint8;
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;

    namespace contract
    {
        struct ShaderGenResult;
    }
}//namespace mtl

using MaterialID            = int;
using MaterialInstanceID    = int;
using ShaderModuleMapByName = UnorderedMap<AnsiString,ShaderModule *>;

constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT = 20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

GRAPH_MODULE_CLASS(MaterialManager)
{
private:

    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    UnorderedMap<AnsiString,Material *> material_by_name;

    AutoIdObjectManager<MaterialID,             Material>           rm_material;                ///<材质合集
    AutoIdObjectManager<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集

private:

    MaterialManager(GraphicsContext *);
    ~MaterialManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    Material *CreateMaterial(const AnsiString &, const mtl::MaterialCreateInfo *);
    Material *CreateMaterialWithContract(const AnsiString &, const mtl::MaterialCreateInfo *, const mtl::contract::ShaderGenRequest *, const mtl::contract::ShaderGenResult *, bool enable_mirror_validation, bool require_mirror_valid, const RendererShaderGenAdapter::DiffLogDetail diff_log_detail);
    class PipelineLayoutData *CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager, const class PipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(Material *mtl, const AnsiString &mtl_name, const mtl::MaterialCreateInfo &mci);
    Material *TryGetCachedMaterial(const AnsiString &name);

public: //Add

    MaterialID              Add(Material *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}

public: //Get

    Material *          GetMaterial         (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}

public: //Release

    void Release(Material *         mtl ){rm_material.Release(mtl);}
    void Release(MaterialInstance * mi  ){rm_material_instance.Release(mi);}

    void Destroy(Material *mtl)
    {
        if (!mtl)
            return;

        const AnsiString &name = mtl->GetName();
        if (!name.IsEmpty())
            material_by_name.DeleteByKey(name);

        rm_material.Release(mtl, true);
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
        // 清理所有材质实例
        if (rm_material_instance.GetCount() > 0)
            rm_material_instance.Clear();

        // 清理所有材质
        if (rm_material.GetCount() > 0)
            rm_material.Clear();

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
    }

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const AnsiString &shader_module_name,
                                                  const VkShaderStageFlagBits stage,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);

public: //ShaderGen Profiler (debug entry, collect-only)

    void ResetShaderGenProfiler()
    {
        RendererShaderGenAdapter::ResetProfiler();
    }

    RendererShaderGenAdapter::ProfilerSnapshot GetShaderGenProfilerSnapshot() const
    {
        return RendererShaderGenAdapter::GetProfilerSnapshot();
    }

    bool GetShaderGenLastValidationReport(RendererShaderGenAdapter::ValidationReport &out_report, std::string *out_material_name = nullptr) const
    {
        return RendererShaderGenAdapter::GetLastValidationReport(out_report, out_material_name);
    }

    std::vector<RendererShaderGenAdapter::ValidationReportRecord> GetShaderGenRecentValidationReports(const uint32_t max_count = 64) const
    {
        return RendererShaderGenAdapter::GetRecentValidationReports(max_count);
    }

    std::map<std::string, std::vector<RendererShaderGenAdapter::ValidationReportRecord>> GetShaderGenRecentValidationReportsByMaterial(const uint32_t max_per_material = 4, const uint32_t max_total = 128) const
    {
        return RendererShaderGenAdapter::GetRecentValidationReportsByMaterial(max_per_material, max_total);
    }

    std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count = 128) const
    {
        return RendererShaderGenAdapter::GetRecentValidationReportCategoryHistogram(max_count);
    }

public: //Material

    Material *          CreateMaterial  (const mtl::MaterialPreset, mtl::Material2DCreateConfig *);  ///<基于内置材质ID创建2D材质
    Material *          CreateMaterial  (const mtl::MaterialPreset, mtl::Material3DCreateConfig *);  ///<基于内置材质ID创建3D材质

public: //MaterialInstance

    MaterialInstance *  CreateMaterialInstance(Material *);
    MaterialInstance *  CreateMaterialInstance(Material *, const VIL *vil);
    MaterialInstance *  CreateMaterialInstance(Material *, const VILConfig *vil_cfg);

    MaterialInstance *  CreateMaterialInstance(Material *, const VIL *vil, const void *, const uint32);
    MaterialInstance *  CreateMaterialInstance(Material *, const VILConfig *vil_cfg, const void *, const uint32);

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl, const VIL *vil, const T *data)
    {
        return CreateMaterialInstance(mtl,vil,data,sizeof(T));
    }

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl, const VILConfig *vil_cfg, const T *data)
    {
        return CreateMaterialInstance(mtl,vil_cfg,data,sizeof(T));
    }

    MaterialInstance *  CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size);
    MaterialInstance *  CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material2DCreateConfig *mcc,const VILConfig *vil_cfg=nullptr)
    {
        return CreateMaterialInstance(mtl_id,mcc,vil_cfg,nullptr,0);
    }

    MaterialInstance *  CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg,const void *data,const uint32 data_size);
    MaterialInstance *  CreateMaterialInstance(const mtl::MaterialPreset mtl_id,mtl::Material3DCreateConfig *mcc,const VILConfig *vil_cfg=nullptr)
    {
        return CreateMaterialInstance(mtl_id,mcc,vil_cfg,nullptr,0);
    }

};//class MaterialManager

}//namespace hgl::graph
