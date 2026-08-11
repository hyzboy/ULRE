#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/ShaderProgramLinkCache.h>
#include<hgl/graph/module/ShaderStageModuleCache.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ObjectManager.h>

namespace hgl::graph{

class GeometryVertexFormat;

namespace mtl
{
    struct MaterialDefinitionBuildRequest;
}//namespace mtl

namespace shadergen
{
    class ShaderCreateInfo;
    class ShaderCreateInfoMap;
    class ShaderProgramBuildSpec;
}//namespace shadergen

using ShaderProgramID = int;

GRAPH_MODULE_CLASS(ShaderProgramManager)
{
private:

    ShaderStageModuleCache shader_module_cache;
    ShaderProgramLinkCache shader_program_cache;

    AutoIdObjectManager<ShaderProgramID, ShaderProgram> rm_material;  ///<材质合集

private:

    VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;   ///< 全局 Bindless Texture Set 布局（Set 3）

    ShaderProgramManager(GraphicsContext *);
    ~ShaderProgramManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    ShaderProgram *AcquireShaderProgram(const shadergen::ShaderProgramKey &, const shadergen::ShaderProgramBuildSpec *);
    class PipelineLayoutData *CreateMaterialPipelineLayoutData(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager);
    class MaterialParameters *CreateMaterialMP(const AnsiString &mtl_name, const class MaterialDescriptorManager *desc_manager, const class PipelineLayoutData *pld, const DescriptorSetType &desc_set_type);
    void ApplyMaterialFinalizePlan(ShaderProgram *mtl, const AnsiString &mtl_name, const shadergen::ShaderProgramBuildSpec &mci);
    ShaderProgram *TryGetCachedShaderProgram(
        const shadergen::ShaderProgramKey &key);
    bool BuildRuntimeShaderProgramState(ShaderProgram *mtl,
                                        const AnsiString &mtl_name,
                                        const shadergen::ShaderProgramBuildSpec *mci,
                                                                        const shadergen::ShaderCreateInfoMap &sci_map);
    bool BuildRuntimeDescriptorState(ShaderProgram *mtl,
                                     const AnsiString &mtl_name,
                                     const shadergen::ShaderProgramBuildSpec *mci);
    bool ExecuteRuntimeMaterialBuildPipeline(ShaderProgram *mtl,
                                             const AnsiString &mtl_name,
                                             const shadergen::ShaderProgramBuildSpec *mci,
                                             const shadergen::ShaderCreateInfoMap &sci_map);

public: //Add

    ShaderProgramID Add(ShaderProgram *mtl) { return rm_material.Add(mtl); }

    /** 设置全局 Bindless Texture Set 布局（必须在创建任何材质前调用）*/
    void SetBindlessLayout(VkDescriptorSetLayout layout) { bindless_layout_ = layout; }

public: //Get

    ShaderProgram *GetShaderProgram(const ShaderProgramID &id) { return rm_material.Get(id); }

public: //Release

    void Release(ShaderProgram *mtl) { rm_material.Release(mtl); }

    void Destroy(ShaderProgram *mtl)
    {
        if (!mtl)
            return;

        shader_program_cache.Remove(mtl->GetProgramKey());

        rm_material.Release(mtl, true);
    }

public: // Override Release from GraphModule - cleanup all resources

    void Release() override
    {
        // 清理所有材质
        if (rm_material.GetCount() > 0)
            rm_material.Clear();

        shader_program_cache.Clear();

        ValueArray<ShaderModule *> shader_modules;
        shader_module_cache.GetValues(shader_modules);
        for (int i = 0; i < shader_modules.GetCount(); ++i)
        {
            delete shader_modules[i];
        }
        shader_module_cache.Clear();
    }

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const shadergen::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModule(const shadergen::ShaderStageKey &, const shadergen::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const AnsiString &shader_module_name,
                                                  const VkShaderStageFlagBits stage,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);
    const ShaderModule *CreateShaderModuleFromSPV(const shadergen::ShaderStageKey &,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);

public: //ShaderProgram

    bool            BuildMaterialResourceLayout(const mtl::MaterialDefinitionBuildRequest &request,
                                                mtl::ShaderResourceSchema &out_layout);
    ShaderProgram *AcquireShaderProgram(
        const mtl::MaterialDefinitionBuildRequest &request);

};//class ShaderProgramManager

}//namespace hgl::graph
