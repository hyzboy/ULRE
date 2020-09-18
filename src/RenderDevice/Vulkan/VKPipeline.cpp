#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    vkDestroyPipeline(device,pipeline,nullptr);
}

Pipeline *CreatePipeline(Device *dev,VKPipelineData *data,const Material *material,const RenderTarget *rt)
{
    VkPipeline graphicsPipeline;
    
    data->InitVertexInputState(material->GetStageCount(),material->GetStages());

    material->Write(data->vis_create_info);

    data->InitViewportState(rt->GetExtent());

    data->pipelineInfo.layout = material->GetPipelineLayout();

    {
        data->pipelineInfo.renderPass = rt->GetRenderPass();
        data->pipelineInfo.subpass = 0;                   //subpass由于还不知道有什么用，所以暂时写0，待知道功用后，需改进
    }

    if (vkCreateGraphicsPipelines(  dev->GetDevice(), 
                                    dev->GetPipelineCache(),
                                    1,&data->pipelineInfo,
                                    nullptr,
                                    &graphicsPipeline) != VK_SUCCESS)
        return(nullptr);

    return(new Pipeline(dev->GetDevice(),graphicsPipeline,data->alpha_test>0,data->alpha_blend));
}
VK_NAMESPACE_END
