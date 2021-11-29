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

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,PipelineData *data)
{
    VkPipeline graphicsPipeline;

    Material *mtl=mi->GetMaterial();

    data->InitShaderStage(mtl->GetStageList());
    data->InitVertexInputState(mi->GetVAB());

    data->SetColorAttachments(color_formats.GetCount());

    data->pipeline_info.layout = mtl->GetPipelineLayout();

    {
        data->pipeline_info.renderPass = render_pass;
        data->pipeline_info.subpass = 0;                   //subpass由于还不知道有什么用，所以暂时写0，待知道功用后，需改进
    }

    if (vkCreateGraphicsPipelines(  device,
                                    pipeline_cache,
                                    1,&data->pipeline_info,
                                    nullptr,
                                    &graphicsPipeline) != VK_SUCCESS)
        return(nullptr);

    return(new Pipeline(device,graphicsPipeline,data));
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip)
{
    PipelineData *pd=GetPipelineData(ip);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip,const Prim &prim,const bool prim_restart)
{
    if(!mi)return(nullptr);

    PipelineData *pd=GetPipelineData(ip);

    pd->Set(prim,prim_restart);

    Pipeline *p=CreatePipeline(mi,pd);

    if(p)
        pipeline_list.Add(p);

    return p;
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,PipelineData *pd,const Prim &prim,const bool prim_restart)
{
    pd->Set(prim,prim_restart);
    
    Pipeline *p=CreatePipeline(mi,pd);

    if(p)
        pipeline_list.Add(p);

    return(p);
}

Pipeline *RenderPass::CreatePipeline(MaterialInstance *mi,const OSString &pipeline_filename,const Prim &prim,const bool prim_restart)
{
    PipelineData *pd=GetPipelineData(pipeline_filename);

    if(!pd)return(nullptr);

    return CreatePipeline(mi,pd,prim,prim_restart);
}
VK_NAMESPACE_END
