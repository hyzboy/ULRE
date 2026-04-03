#include<hgl/vk/pipeline/VKRenderFormat.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<cstdio>
#include<atomic>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/log/Log.h>

namespace {
    std::atomic<uint64_t> g_rf_vkcreate_count{0};
} // anonymous namespace

namespace hgl::graph{

RenderFormat::RenderFormat(VulkanDevice *dev,const AnsiString &n,const VkFormatList &cf,VkFormat df)
{
    device=dev;
    name=n;
    pipeline_cache=dev->GetPipelineCache();
    color_formats=cf;
    depth_format=df;

    LogInfo("[RenderFormat::RenderFormat] Created RenderFormat '%s', color attachment count=%u, depth format=%u",
             name.c_str(), color_formats.GetCount(), depth_format);
}

RenderFormat::~RenderFormat()
{
    LogInfo("[RenderFormat::~RenderFormat] Destroying RenderFormat '%s' (RenderFormat*=0x%llx)",
             name.c_str(), (unsigned long long)(uintptr_t)this);
}

GraphicsPipeline *RenderFormat::CreatePipeline(const AnsiString &name,GraphicsPipelineData *pd,const ShaderStageCreateInfoList &ssci_list,VkPipelineLayout pl,const VIL *vil)
{
    HGL_CAPTURE_SCOPE();

    //以后要做一个缓冲，以Material为基准创建一个pipeline，其它MaterialInstance的pipeline全部以它为基础，这样可以提升性能。

    VkPipeline graphicsPipeline;

    pd->InitShaderStage(ssci_list);
    pd->InitVertexInputState(vil);

    pd->SetColorAttachments(color_formats.GetCount());

    pd->pipeline_info.layout = pl;

    {
        VkPipelineRenderingCreateInfoKHR rendering_ci = {};
        rendering_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering_ci.colorAttachmentCount = (uint32_t)color_formats.GetCount();
        rendering_ci.pColorAttachmentFormats = color_formats.GetData();
        rendering_ci.depthAttachmentFormat = depth_format;
        pd->pipeline_info.pNext = &rendering_ci;
        pd->pipeline_info.renderPass = VK_NULL_HANDLE;
        pd->pipeline_info.subpass = 0;
    }

    if (vkCreateGraphicsPipelines(  *device,
        pipeline_cache,
        1,&pd->pipeline_info,
        nullptr,
        &graphicsPipeline) != VK_SUCCESS)
    {
        //有一种常见问题就是GraphicsPipelineData未调用SetPrim

        delete pd;
        return(nullptr);
    }

    ++g_rf_vkcreate_count;

    GraphicsPipeline *pipeline = new GraphicsPipeline(name,*device,graphicsPipeline,vil,pd);

    LogInfo("[RenderFormat::CreatePipeline] Created GraphicsPipeline '%s' in RenderFormat '%s' (VkPipeline=0x%llx, GraphicsPipeline*=0x%llx)",
             name.c_str(), this->name.c_str(), (unsigned long long)(uintptr_t)graphicsPipeline, (unsigned long long)(uintptr_t)pipeline);

    if (device)
        device->TrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)graphicsPipeline, ObjectNameBuilder(name).Append(ObjectTypeTag::VKPipeline));

    return pipeline;
}

GraphicsPipeline *RenderFormat::CreatePipeline(Material *mtl,const VIL *vil,const GraphicsPipelineData *cpd,const bool prim_restart)
{
    if (!mtl || !vil || !cpd)
        return nullptr;

    if (!device)
        return nullptr;

    GraphicsPipelineBuildRequest req;
    req.material       = mtl;
    req.vil            = vil;
    req.render_format  = this;
    req.pipeline_data  = cpd;
    req.primitive      = mtl->GetPrimitiveType();
    req.primitive_restart = prim_restart;
    req.debug_name     = mtl->GetName();

    GraphicsPipeline *p = device->AcquireGraphicsPipeline(req);
    // Ownership of p is held by VulkanDevice::linked_pipeline_cache.
    // RenderFormat does NOT add it to pipeline_list.
    return p;
}

GraphicsPipeline *RenderFormat::CreatePipeline(Material *mtl,const VIL *vil,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    if(!mtl)return(nullptr);

    return CreatePipeline(mtl,vil,GetGraphicsPipelineData(ip),prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(Material *mtl,const GraphicsPipelineData *pd,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),pd,prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(Material *mtl,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),ip,prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(MaterialInstance *mi,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    if(!mi)return(nullptr);

    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),ip,prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(MaterialInstance *mi,const GraphicsPipelineData *cpd,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),cpd,prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const bool prim_restart)
{
    const GraphicsPipelineData *pd=GetGraphicsPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim_restart);
}

GraphicsPipeline *RenderFormat::CreatePipeline(const AnsiString &name,
                                       const ShaderStageCreateInfoList &ssci,
                                       VkPipelineLayout layout,
                                       const VIL *vil,
                                       const GraphicsPipelineData *cpd,
                                       PrimitiveType prim,
                                       bool prim_restart)
{
    GraphicsPipelineData *pd = new GraphicsPipelineData(cpd);

    pd->SetPrim(prim, prim_restart);

    GraphicsPipeline *p = CreatePipeline(name, pd, ssci, layout, vil);

    return p;
}

uint64_t RenderFormat::GetVkCreateCount()
{
    return g_rf_vkcreate_count.load(std::memory_order_relaxed);
}

void RenderFormat::IncrVkCreateCount()
{
    ++g_rf_vkcreate_count;
}
}//namespace hgl::graph
