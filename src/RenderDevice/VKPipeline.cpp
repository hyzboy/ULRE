#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    vkDestroyPipeline(device,pipeline,nullptr);
}

Pipeline *CreatePipeline(VkDevice device,VkPipelineCache pipeline_cache,PipelineData *data,const Material *material,const RenderTarget *rt)
{
    VkPipeline graphicsPipeline;
    
    data->InitVertexInputState( material->GetStageCount(),
                                material->GetStages(),
                                material->GetVertexAttrCount(),
                                material->GetVertexBindingList(),
                                material->GetVertexAttributeList());

    data->InitViewportState(rt->GetExtent());

    data->pipeline_info.layout = material->GetPipelineLayout();

    {
        data->pipeline_info.renderPass = rt->GetRenderPass();
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

Pipeline *Device::CreatePipeline(const InlinePipeline &ip,const Material *mtl,const RenderTarget *rt)
{
    PipelineData *pd=GetPipelineData(ip);

    if(!pd)return(nullptr);

    return VK_NAMESPACE::CreatePipeline(attr->device,attr->pipeline_cache,pd,mtl,rt);
}

Pipeline *Device::CreatePipeline(PipelineData *pd,const Material *mtl,const RenderTarget *rt)
{
    return VK_NAMESPACE::CreatePipeline(attr->device,attr->pipeline_cache,pd,mtl,rt);
}
VK_NAMESPACE_END
