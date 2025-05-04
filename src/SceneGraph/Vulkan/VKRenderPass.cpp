#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKPipelineData.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN
RenderPass::RenderPass(VkDevice d,VkPipelineCache pc,VkRenderPass rp,const ArrayList<VkFormat> &cf,VkFormat df)
{
    device=d;
    pipeline_cache=pc;
    render_pass=rp;
    color_formats=cf;
    depth_format=df;

    vkGetRenderAreaGranularity(device,render_pass,&granularity);
}

RenderPass::~RenderPass()
{
    pipeline_list.Clear();

    vkDestroyRenderPass(device,render_pass,nullptr);
}

Pipeline *RenderPass::CreatePipeline(const AnsiString &name,PipelineData *pd,const ShaderStageCreateInfoList &ssci_list,VkPipelineLayout pl,const VIL *vil)
{
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

    if (vkCreateGraphicsPipelines(  device,
        pipeline_cache,
        1,&pd->pipeline_info,
        nullptr,
        &graphicsPipeline) != VK_SUCCESS)
    {
        //有一种常见问题就是PipelineData未调用SetPrim

        delete pd;
        return(nullptr);
    }

    return(new Pipeline(name,device,graphicsPipeline,vil,pd));
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const VIL *vil,const PipelineData *cpd,const PrimitiveType &prim,const bool prim_restart)
{
    PipelineData *pd=new PipelineData(cpd);

    pd->SetPrim(prim,prim_restart);
    
    Pipeline *p=CreatePipeline(mtl->GetName(),pd,mtl->GetStageList(),mtl->GetPipelineLayout(),vil);

    if(p)
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const VIL *vil,const InlinePipeline &ip,const PrimitiveType &prim,const bool prim_restart)
{
    if(!mtl)return(nullptr);

    return CreatePipeline(mtl,vil,GetPipelineData(ip),prim,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const PipelineData *pd,   const PrimitiveType &prim,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),pd,prim,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(Material *mtl,const InlinePipeline &ip, const PrimitiveType &prim,const bool prim_restart)
{
    return CreatePipeline(mtl,mtl->GetDefaultVIL(),ip,prim,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip,const PrimitiveType &prim,const bool prim_restart)
{
    if(!mi)return(nullptr);
    
    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),ip,prim,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const PipelineData *cpd,const PrimitiveType &prim,const bool prim_restart)
{
    return CreatePipeline(mi->GetMaterial(),mi->GetVIL(),cpd,prim,prim_restart);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const PrimitiveType &prim,const bool prim_restart)
{
    const PipelineData *pd=GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim,prim_restart);
}
VK_NAMESPACE_END
