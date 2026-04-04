#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
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

RenderTargetFormat::RenderTargetFormat(VulkanDevice *dev,const AnsiString &n,const VkFormatList &cf,VkFormat df)
{
    device=dev;
    name=n;
    pipeline_cache=dev->GetPipelineCache();
    color_formats=cf;
    depth_format=df;

    LogInfo("[RenderTargetFormat::RenderTargetFormat] Created RenderTargetFormat '%s', color attachment count=%u, depth format=%u",
             name.c_str(), color_formats.GetCount(), depth_format);
}

RenderTargetFormat::~RenderTargetFormat()
{
    LogInfo("[RenderTargetFormat::~RenderTargetFormat] Destroying RenderTargetFormat '%s' (RenderTargetFormat*=0x%llx)",
             name.c_str(), (unsigned long long)(uintptr_t)this);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(Material *mtl,const VIL *vil,const GraphicsPipelineData *cpd,const bool prim_restart)
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
    // RenderTargetFormat does NOT add it to pipeline_list.
    return p;
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(Material *mtl,const VIL *vil,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    if(!mtl)return(nullptr);

    return CreatePipeline(mtl,vil,GetGraphicsPipelineData(ip),prim_restart);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(Material *mtl,const GraphicsPipelineData *pd,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),pd,prim_restart);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(Material *mtl,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),ip,prim_restart);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(MaterialInstance *mi,const GraphicsPipelinePreset &ip,const bool prim_restart)
{
    if(!mi)return(nullptr);

    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),ip,prim_restart);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(MaterialInstance *mi,const GraphicsPipelineData *cpd,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),cpd,prim_restart);
}

GraphicsPipeline *RenderTargetFormat::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const bool prim_restart)
{
    const GraphicsPipelineData *pd=GetGraphicsPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim_restart);
}

uint64_t RenderTargetFormat::GetVkCreateCount()
{
    return g_rf_vkcreate_count.load(std::memory_order_relaxed);
}

void RenderTargetFormat::IncrVkCreateCount()
{
    ++g_rf_vkcreate_count;
}
}//namespace hgl::graph
