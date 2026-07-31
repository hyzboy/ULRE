#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ObjectManager.h>
#include<hgl/graph/module/ShaderGenValidationTypes.h>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph{

class ShaderCreateInfo;
class ShaderCreateInfoMap;
class GeometryVertexFormat;

namespace mtl
{
    struct MaterialDefinitionBuildRequest;
    class ShaderProgramBuildSpec;
}//namespace mtl

using MaterialID            = int;
using MaterialInstanceID    = int;
using ShaderModuleMapByName = UnorderedMap<AnsiString,ShaderModule *>;

constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT = 20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

GRAPH_MODULE_CLASS(MaterialManager)
{
private:

    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    UnorderedMap<AnsiString,ShaderProgram *> material_by_name;

    AutoIdObjectManager<MaterialID,             ShaderProgram>           rm_material;                ///<材质合集
    AutoIdObjectManager<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集

private:

    VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;   ///< 全局 Bindless Texture Set 布局（Set 4）

    MaterialManager(GraphicsContext *);
    ~MaterialManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    ShaderProgram *AcquireMaterialProgram(const AnsiString &, const mtl::ShaderProgramBuildSpec *);
    class PipelineLayoutData *CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager, const class PipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const mtl::ShaderProgramBuildSpec &mci);
    ShaderProgram *TryGetCachedMaterial(const AnsiString &name);
    bool ExecuteMaterialBuildPipeline(ShaderProgram *mtl,
                                      const AnsiString &mtl_name,
                                      const mtl::ShaderProgramBuildSpec *mci,
                                      const ShaderCreateInfoMap &sci_map);

public: //Add

    MaterialID              Add(ShaderProgram *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}

    /** 设置全局 Bindless Texture Set 布局（必须在创建任何材质前调用）*/
    void SetBindlessLayout(VkDescriptorSetLayout layout) { bindless_layout_ = layout; }

public: //Get

    ShaderProgram *          GetMaterialProgram         (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}

public: //Release

    void Release(ShaderProgram *         mtl ){rm_material.Release(mtl);}
    void Release(MaterialInstance * mi  ){rm_material_instance.Release(mi);}

    void Destroy(ShaderProgram *mtl)
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

    void ResetShaderGenProfiler();

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshot() const;

    bool GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name = nullptr) const;

    std::vector<ShaderGenValidationReportRecord> GetShaderGenRecentValidationReports(const uint32_t max_count = 64) const;

    std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogram(const uint32_t max_count = 128) const;

public: //ShaderProgram

    ShaderProgram *   AcquireMaterialProgram(const mtl::MaterialDefinitionBuildRequest &request);
    ShaderProgram *   AcquireMaterialProgram(const std::string &mtl_def_id,
                                                  const mtl::MaterialRecipe &recipe,
                                                  PrimitiveType prim_type,
                                                  const GeometryVertexFormat *geometry_vertex_format = nullptr);

private:

    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *);
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const VIL *vil);
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const VILConfig *vil_cfg);
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const GeometryVertexFormat &geometry_vertex_format);
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const GeometryVertexFormat &geometry_vertex_format, const void *, const uint32);

    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const VIL *vil, const void *, const uint32);
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *, const VILConfig *vil_cfg, const void *, const uint32);

    template<typename T>
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *mtl, const VIL *vil, const T *data)
    {
        return CreateMaterialInstanceInternal(mtl,vil,data,sizeof(T));
    }

    template<typename T>
    MaterialInstance *  CreateMaterialInstanceInternal(ShaderProgram *mtl, const VILConfig *vil_cfg, const T *data)
    {
        return CreateMaterialInstanceInternal(mtl,vil_cfg,data,sizeof(T));
    }

};//class MaterialManager

}//namespace hgl::graph
