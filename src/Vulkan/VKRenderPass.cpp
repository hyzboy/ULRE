#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<hgl/vk/pipeline/VKPipelineDataBuild.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKPipelineResolver.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
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
                                      PipelineData *pd,
                                      const ShaderStageCreateInfoList &ssci_list,
                                      VkPipelineLayout pl,
                                      const GeometryVertexFormat *gvf,
                                      const uint64_t pipeline_config_hash,
                                      const bool is_overlay)
{
    HGL_CAPTURE_SCOPE();

    FinalPipelineResolveRequest request{};
    request.device = device;
    request.pipeline_cache = pipeline_cache;
    request.subpass = 0;
    request.frame_output.color_formats = color_formats.GetData();
    request.frame_output.color_attachment_count = color_formats.GetCount();
    request.frame_output.depth_stencil_format = depth_format;
    request.debug_name = &name;
    request.pipeline_config_hash = pipeline_config_hash;
    request.pipeline_data = pd;
    request.shader_stages = &ssci_list;
    request.pipeline_layout = pl;
    request.geometry_vertex_format = gvf;

    FinalPipelineResolveResult resolve_result{};
    if(!PipelineResolver::ResolveFinalPipeline(request, resolve_result))
    {
        delete pd;
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
            delete pd;
            LogDebug("[RenderPass::CreatePipeline] Reuse existing Pipeline wrapper '%s' in RenderPass '%s' (VkPipeline=0x%llx, Pipeline*=0x%llx)",
                     existing_pipeline->GetName().c_str(),
                     this->name.c_str(),
                     (unsigned long long)(uintptr_t)graphicsPipeline,
                     (unsigned long long)(uintptr_t)existing_pipeline);
            return existing_pipeline;
        }
    }

    Pipeline *pipeline = new Pipeline(name,*device,graphicsPipeline,pd,is_overlay);

    const char *mode_name = resolve_result.materialize_mode == PipelineMaterializeMode::GraphicsPipelineLibrary
                          ? "GPL"
                          : "Monolithic";

    LogInfo("[RenderPass::CreatePipeline] Created Pipeline '%s' in RenderPass '%s' (VkPipeline=0x%llx, mode=%s, Pipeline*=0x%llx)",
             name.c_str(), this->name.c_str(), (unsigned long long)(uintptr_t)graphicsPipeline, mode_name, (unsigned long long)(uintptr_t)pipeline);

    if (device)
        device->TrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)graphicsPipeline, ObjectNameBuilder(name).Append(ObjectTypeTag::VKPipeline));

    return pipeline;
}

Pipeline *RenderPass::CreatePipeline(ShaderProgram *mtl,const PipelineData *cpd,const bool prim_restart,const GeometryVertexFormat *gvf)
{
    PipelineData *pd=new PipelineData(cpd);

    pd->SetPrim(mtl->GetPrimitiveType(),prim_restart);

    Pipeline *p=CreatePipeline(mtl->GetName(),pd,mtl->GetStageList(),mtl->GetPipelineLayout(),gvf,0,false);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderPass::CreatePipeline(ShaderProgram *mtl,const mtl::MaterialPipelineConfig &config,const bool prim_restart,const GeometryVertexFormat *gvf)
{
    if(!mtl)
        return(nullptr);

    PipelineData *pd = BuildPipelineData(config);
    pd->SetPrim(mtl->GetPrimitiveType(),prim_restart);

    Pipeline *p = CreatePipeline(mtl->GetName(),
                                  pd,
                                  mtl->GetStageList(),
                                  mtl->GetPipelineLayout(),
                                  gvf,
                                  mtl::HashMaterialPipelineConfig(config),
                                  config.overlay);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderPass::CreatePipeline(ShaderProgram *mtl,const mtl::MaterialRecipe &recipe,const bool prim_restart,const GeometryVertexFormat *gvf)
{
    if(!mtl)
        return(nullptr);

    mtl::MaterialRecipe normalized_recipe = recipe;
    mtl::NormalizeRecipe(normalized_recipe);

    mtl::MaterialDefinition definition{};
    if(!mtl::TryGetMaterialDefinitionByID(normalized_recipe.mtl_def_id, definition))
    {
        GLogError(u8"[RenderPass::CreatePipeline] MaterialDefinition not found: '%s'",
                  normalized_recipe.mtl_def_id.c_str());
        return nullptr;
    }
    mtl::ResolvedMaterialRenderState render_state =
        mtl::ResolveMaterialRenderState(definition, normalized_recipe);

    // Lines 图元（LineQuad mesh shader 输出 quad 三角形）：绕序不定——强制双面绘制
    //（cull off；在 hash 前修改——管线缓存 key 一致）
    if (mtl->GetPrimitiveType() == PrimitiveType::Lines)
        render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;

    PipelineData *pd = BuildPipelineData(render_state);
    pd->SetPrim(mtl->GetPrimitiveType(),prim_restart);

    Pipeline *p = CreatePipeline(mtl->GetName(),
                                  pd,
                                  mtl->GetStageList(),
                                  mtl->GetPipelineLayout(),
                                  gvf,
                                  mtl::HashResolvedMaterialRenderState(render_state),
                                  render_state.pipeline_config.overlay);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderPass::CreatePipeline(const AnsiString &name,
                                     const ShaderStageCreateInfoList &ssci,
                                     VkPipelineLayout layout,
                                     const PipelineData *cpd,
                                     PrimitiveType prim,
                                     bool prim_restart,
                                     const GeometryVertexFormat *gvf)
{
    PipelineData *pd = new PipelineData(cpd);

    pd->SetPrim(prim, prim_restart);

    Pipeline *p = CreatePipeline(name, pd, ssci, layout, gvf, 0, false);

    if(p && !pipeline_list.Contains(p))
        pipeline_list.Add(p);

    return p;
}
}//namespace hgl::graph
