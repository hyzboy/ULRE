#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/log/Log.h>
namespace hgl::graph{
RenderPass::RenderPass(VulkanDevice *dev,const AnsiString &n,VkRenderPass rp,const VkFormatList &cf,VkFormat df)
{
    device=dev;
    name=n;
    pipeline_cache=dev->GetPipelineCache();
    render_pass=rp;
    color_formats=cf;
    depth_format=df;

    vkGetRenderAreaGranularity(*device,render_pass,&granularity);

    LogInfo("[RenderPass::RenderPass] Created RenderPass '%s' with VkRenderPass=0x%llx, color attachment count=%u, depth format=%u, granularity=(%ux%u)",
             name.c_str(), (unsigned long long)(uintptr_t)render_pass, color_formats.GetCount(), depth_format,
             granularity.width, granularity.height);
}

RenderPass::~RenderPass()
{
    LogInfo("[RenderPass::~RenderPass] Destroying RenderPass with %u pipelines (VkRenderPass=0x%llx, RenderPass*=0x%llx)",
             (unsigned int)pipeline_list.GetCount(), (unsigned long long)(uintptr_t)render_pass, (unsigned long long)(uintptr_t)this);

    // 列出所有要被销毁的管道
    for (size_t i = 0; i < pipeline_list.GetCount(); i++)
    {
        Pipeline* p = pipeline_list[i];
        if (p)
            LogInfo("  [RenderPass::~RenderPass] Clearing Pipeline [%zu]: '%s'", i, p->GetName().c_str());
    }

    LogDebug("[RenderPass::~RenderPass] Clearing pipeline_list...");
    pipeline_list.Clear();
    LogDebug("[RenderPass::~RenderPass] Pipelines cleared, now destroying VkRenderPass");

    if (device)
        device->UntrackObject(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)(uintptr_t)render_pass);

    vkDestroyRenderPass(*device,render_pass,nullptr);
    LogInfo("[RenderPass::~RenderPass] RenderPass destroyed");
}

Pipeline *RenderPass::CreatePipeline(const AnsiString &name,
                                      const ShaderStageCreateInfoList &ssci_list,
                                      VkPipelineLayout pl,
                                      const mtl::MaterialPipelineConfig &config)
{
    HGL_CAPTURE_SCOPE();

    FinalPipelineResolveRequest request{};
    request.device = device;
    request.pipeline_cache = pipeline_cache;
    request.frame_output.color_formats = color_formats.GetData();
    request.frame_output.color_attachment_count = color_formats.GetCount();
    request.frame_output.depth_stencil_format = depth_format;
    request.debug_name = &name;
    request.shader_stages = &ssci_list;
    request.pipeline_layout = pl;

    FinalPipelineResolveResult resolve_result{};
    if(!PipelineResolver::ResolveFinalPipeline(request, resolve_result))
    {
        return(nullptr);
    }

    VkPipeline graphicsPipeline = resolve_result.pipeline;

    // Resolver may return an already-cached VkPipeline handle for identical final keys.
    // Reuse existing wrapper to keep one owner per VkPipeline handle.
    for(int i=0;i<pipeline_list.GetCount();++i)
    {
        Pipeline *existing_pipeline = pipeline_list[i];
        if(existing_pipeline && VkPipeline(*existing_pipeline) == graphicsPipeline)
        {
            PipelineResolver::ReleaseFinalPipeline(device, graphicsPipeline);
            LogDebug("[RenderPass::CreatePipeline] Reuse existing Pipeline wrapper '%s' in RenderPass '%s' (VkPipeline=0x%llx, Pipeline*=0x%llx)",
                     existing_pipeline->GetName().c_str(),
                     this->name.c_str(),
                     (unsigned long long)(uintptr_t)graphicsPipeline,
                     (unsigned long long)(uintptr_t)existing_pipeline);
            return existing_pipeline;
        }
    }

    Pipeline *pipeline = new Pipeline(name,*device,graphicsPipeline,config,config.overlay);

    LogInfo("[RenderPass::CreatePipeline] Created Pipeline '%s' in RenderPass '%s' (VkPipeline=0x%llx, Pipeline*=0x%llx)",
             name.c_str(), this->name.c_str(), (unsigned long long)(uintptr_t)graphicsPipeline, (unsigned long long)(uintptr_t)pipeline);

    if (device)
        device->TrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)graphicsPipeline, ObjectNameBuilder(name).Append(ObjectTypeTag::VKPipeline));

    return pipeline;
}

Pipeline *RenderPass::CreatePipeline(ShaderProgram *mtl,const mtl::MaterialPipelineConfig &config)
{
    if(!mtl)
        return(nullptr);

    Pipeline *p = CreatePipeline(mtl->GetName(),
                                  mtl->GetStageList(),
                                  mtl->GetPipelineLayout(),
                                  config);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderPass::CreatePipeline(ShaderProgram *mtl,const mtl::MaterialRecipe &recipe)
{
    if(!mtl)
        return(nullptr);

    // recipe 必须已完成 NormalizeRecipe（组件/acquire 边界负责），此处直接从
    // 规范化 overrides 重建渲染状态，不再反查 MaterialDefinition——Vulkan 层
    // 由此不依赖材质注册表。
    if(!mtl::IsRecipeNormalized(recipe))
    {
        GLogError(u8"[RenderPass::CreatePipeline] recipe not normalized (call NormalizeRecipe at the authoring/acquire boundary): '%s'",
                  recipe.mtl_def_id.c_str());
        return nullptr;
    }
    mtl::ResolvedMaterialRenderState render_state =
        mtl::GetNormalizedRecipeRenderState(recipe);

    // Lines 图元（LineQuad mesh shader 输出 quad 三角形）：绕序不定——强制双面绘制
    //（cull off）
    if (mtl->GetPrimitiveType() == PrimitiveType::Lines)
        render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;

    // double_sided 合并进 config（渲染侧 EDS 动态状态只读 config）
    if (render_state.double_sided)
        render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;

    Pipeline *p = CreatePipeline(mtl->GetName(),
                                  mtl->GetStageList(),
                                  mtl->GetPipelineLayout(),
                                  render_state.pipeline_config);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return p;
}
}//namespace hgl::graph
