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
class ComputePipeline;

namespace mtl
{
    struct MaterialDefinitionBuildRequest;
    struct MaterialDefinition;
    struct RenderTemplateRequest;
}//namespace mtl

namespace mtl
{
    class ShaderCreateInfo;
    class ShaderCreateInfoMap;
    class ShaderBuildContext;
}//namespace mtl

using ShaderProgramID = int;

// SceneGraph policy for the existing material provider shapes. Callers that
// need a route selected from material data use this before acquiring a program.
bool SelectCurrentSceneRenderTemplateRequest(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialDefinitionBuildRequest &request,
    mtl::RenderTemplateRequest &out_request);

GRAPH_MODULE_CLASS(ShaderProgramManager)
{
private:

    ShaderStageModuleCache shader_module_cache;
    ShaderProgramLinkCache shader_program_cache;

    AutoIdObjectManager<ShaderProgramID, ShaderProgram> rm_material;  ///<材质合集

private:

    VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;   ///< 全局 Bindless Texture Set 布局（Set 1）
    VkDescriptorSetLayout scene_layout_    = VK_NULL_HANDLE;   ///< 全局 Scene UBO Set 布局（Set 0，设备级）
    VkPipelineLayout shared_pipeline_layout_ = VK_NULL_HANDLE;  ///<全材质共享 pipeline layout 单例（惰建，由本模块拥有，Release() 时销毁）

    ShaderProgramManager(GraphicsContext *);
    ~ShaderProgramManager()=default;

    friend class GraphModuleManager;

private: // Helper methods with integrated DebugUtils

    ShaderProgram *AcquireShaderProgram(const mtl::ShaderProgramKey &, const mtl::ShaderBuildContext *);
    ShaderProgram *TryGetCachedShaderProgram(
        const mtl::ShaderProgramKey &key);
    VkPipelineLayout GetOrCreateGlobalPipelineLayout();
    bool BuildRuntimeShaderProgramState(ShaderProgram *mtl,
                                        const AnsiString &mtl_name,
                                        const mtl::ShaderBuildContext *ctx,
                                        const mtl::ShaderCreateInfoMap &sci_map);
    bool ExecuteRuntimeMaterialBuildPipeline(ShaderProgram *mtl,
                                             const AnsiString &mtl_name,
                                             const mtl::ShaderBuildContext *ctx,
                                             const mtl::ShaderCreateInfoMap &sci_map);

public: //Add

    ShaderProgramID Add(ShaderProgram *mtl) { return rm_material.Add(mtl); }

    /** 设置全局 Bindless Texture Set 布局（必须在创建任何材质前调用）*/
    void SetBindlessLayout(VkDescriptorSetLayout layout) { bindless_layout_ = layout; }

    /** 设置全局 Scene UBO Set 布局（必须在创建任何材质前调用，P1）*/
    void SetSceneLayout(VkDescriptorSetLayout layout) { scene_layout_ = layout; }

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

    void Release() override;

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const mtl::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModule(const mtl::ShaderStageKey &, const mtl::ShaderCreateInfo *);
    const ShaderModule *CreateShaderModuleFromSPV(const AnsiString &shader_module_name,
                                                  const VkShaderStageFlagBits stage,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);
    const ShaderModule *CreateShaderModuleFromSPV(const mtl::ShaderStageKey &,
                                                  const uint32_t *spv_data,
                                                  const size_t spv_size);

public: //Compute（直接源码路径，不走 ShaderGen 生成器）

    /** 编译 compute GLSL 源码并创建 ShaderModule（按 name 缓存，与 graphics 模块同一缓存） */
    const ShaderModule *CreateComputeShaderModule(const AnsiString &shader_module_name, const AnsiString &glsl_source);

    /**
     * 直接源码创建 compute 管线：GLSL 源码 → 编译 → ShaderModule → ComputePipeline
     * 专用 layout：Set0=全局 Scene 集 / Set1=全局 Bindless 集 / Set2=用户集（可选，自己的 UBO/SSBO）
     * push constant：COMPUTE stage、offset 0、大小由 push_constant_size 指定（≤128B，spec 保证值）
     * 返回的 ComputePipeline 由调用方 delete（析构时连带销毁专用 layout，不影响共享全局 layout）
     */
    ComputePipeline *CreateComputePipeline(const AnsiString &name,
                                           const AnsiString &glsl_source,
                                           VkDescriptorSetLayout user_layout = VK_NULL_HANDLE,
                                           const uint32_t push_constant_size = 0);

public: //ShaderProgram

    bool            BuildShaderResourceSchema(const mtl::MaterialDefinitionBuildRequest &request,
                                                mtl::ShaderResourceSchema &out_schema);
    ShaderProgram *AcquireShaderProgram(
        const mtl::MaterialDefinitionBuildRequest &request);

};//class ShaderProgramManager

}//namespace hgl::graph
