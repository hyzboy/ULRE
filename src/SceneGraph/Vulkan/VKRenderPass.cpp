#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/graph/VKPipelineData.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
VK_NAMESPACE_BEGIN
RenderPass::RenderPass(VkDevice d,VkPipelineCache pc,VkRenderPass rp,const List<VkFormat> &cf,VkFormat df)
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

Pipeline *RenderPass::CreatePipeline(Material *mtl,PipelineData *pd,const VAB *vab)
{
    VkPipeline graphicsPipeline;

    pd->InitShaderStage(mtl->GetStageList());
    pd->InitVertexInputState(vab);

    pd->SetColorAttachments(color_formats.GetCount());

    pd->pipeline_info.layout = mtl->GetPipelineLayout();

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
        delete pd;
        return(nullptr);
    }

    return(new Pipeline(device,graphicsPipeline,pd));
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const PipelineData *data)
{
    Material *mtl=mi->GetMaterial();

    PipelineData *pd=new PipelineData(data);

    return CreatePipeline(mtl,pd,mi->GetVAB());
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip)
{
    const PipelineData *pd=GetPipelineData(ip);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip,const Prim &prim,const bool prim_restart)
{
    if(!mi)return(nullptr);

    const PipelineData *cpd=GetPipelineData(ip);

    PipelineData *pd=new PipelineData(cpd);

    pd->Set(prim,prim_restart);

    Pipeline *p=CreatePipeline(mi->GetMaterial(),pd,mi->GetVAB());

    if(p)
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const PipelineData *cpd,const Prim &prim,const bool prim_restart)
{
    PipelineData *pd=new PipelineData(cpd);

    pd->Set(prim,prim_restart);

    Pipeline *p=CreatePipeline(mi->GetMaterial(),pd,mi->GetVAB());

    if(p)
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const Prim &prim,const bool prim_restart)
{
    const PipelineData *pd=GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim,prim_restart);
}
VK_NAMESPACE_END
