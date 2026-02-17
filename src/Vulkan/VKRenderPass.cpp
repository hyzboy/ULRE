#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/utils/ObjectTracker.h>
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
}

RenderPass::~RenderPass()
{
    std::cout << "[RenderPass::~RenderPass] Destroying RenderPass with " << pipeline_list.GetCount() 
              << " pipelines (VkRenderPass=0x" << std::hex << (uintptr_t)render_pass << std::dec 
              << ", RenderPass*=0x" << (uintptr_t)this << ")" << std::endl;
    
    // 列出所有要被销毁的管道
    for (size_t i = 0; i < pipeline_list.GetCount(); i++)
    {
        Pipeline* p = pipeline_list[i];
        if (p)
            std::cout << "  [RenderPass::~RenderPass] Clearing Pipeline [" << i << "]: '" << p->GetName() << "'" << std::endl;
    }
    
    std::cout << "[RenderPass::~RenderPass] Clearing pipeline_list..." << std::endl;
    pipeline_list.Clear();
    std::cout << "[RenderPass::~RenderPass] Pipelines cleared, now destroying VkRenderPass" << std::endl;

    if (device)
        device->UntrackObject(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)(uintptr_t)render_pass);

    vkDestroyRenderPass(*device,render_pass,nullptr);
    std::cout << "[RenderPass::~RenderPass] RenderPass destroyed" << std::endl;
}

Pipeline *RenderPass::CreatePipeline(const AnsiString &name,PipelineData *pd,const ShaderStageCreateInfoList &ssci_list,VkPipelineLayout pl,const VIL *vil)
{
    HGL_CAPTURE_SCOPE();

    //以后要做一个缓冲，以Material为基准创建一个pipeline，其它MaterialInstance的pipeline全部以它为基础，这样可以提升性能。

    VkPipeline graphicsPipeline;

    pd->InitShaderStage(ssci_list);
    pd->InitVertexInputState(vil);

    pd->SetColorAttachments(color_formats.GetCount());

    pd->pipeline_info.layout = pl;

    {
        pd->pipeline_info.renderPass = render_pass;
        pd->pipeline_info.subpass = 0;                   //subpass由于还不知道有什么用，所以暂时写0，待知道功用后，需改进
    }

    if (vkCreateGraphicsPipelines(  *device,
        pipeline_cache,
        1,&pd->pipeline_info,
        nullptr,
        &graphicsPipeline) != VK_SUCCESS)
    {
        //有一种常见问题就是PipelineData未调用SetPrim

        delete pd;
        return(nullptr);
    }

    Pipeline *pipeline = new Pipeline(name,*device,graphicsPipeline,vil,pd);

    std::cout << "[RenderPass::CreatePipeline] Created Pipeline '" << name << "' in RenderPass '" << this->name 
              << "' (VkPipeline=0x" << std::hex << (uintptr_t)graphicsPipeline << std::dec << ", Pipeline*=0x" 
              << (uintptr_t)pipeline << ")" << std::endl;

    if (device)
        device->TrackObject(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)graphicsPipeline, ObjectNameBuilder(name).Append(ObjectTypeTag::VKPipeline));

    return pipeline;
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const VIL *vil,const PipelineData *cpd,const bool prim_restart)
{
    PipelineData *pd=new PipelineData(cpd);

    pd->SetPrim(mtl->GetPrimitiveType(),prim_restart);

    Pipeline *p=CreatePipeline(mtl->GetName(),pd,mtl->GetStageList(),mtl->GetPipelineLayout(),vil);

    if(p)
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const VIL *vil,const InlinePipeline &ip,const bool prim_restart)
{
    if(!mtl)return(nullptr);

    return CreatePipeline(mtl,vil,GetPipelineData(ip),prim_restart);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const PipelineData *pd,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),pd,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const InlinePipeline &ip,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),ip,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip,const bool prim_restart)
{
    if(!mi)return(nullptr);

    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),ip,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const PipelineData *cpd,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),cpd,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const bool prim_restart)
{
    const PipelineData *pd=GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim_restart);
}
}//namespace hgl::graph
