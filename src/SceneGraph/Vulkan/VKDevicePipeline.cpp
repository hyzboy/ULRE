#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN
Pipeline *GPUDevice::CreatePipeline(PipelineData *data,const Material *material,const RenderTarget *rt)
{
    VkPipeline graphicsPipeline;
    
    data->InitVertexInputState( material->GetStageCount(),
                                material->GetStages(),
                                material->GetVertexAttrCount(),
                                material->GetVertexBindingList(),
                                material->GetVertexAttributeList());

    data->InitViewportState(rt->GetExtent());
    
    data->SetColorAttachments(rt->GetColorCount());

    data->pipeline_info.layout = material->GetPipelineLayout();

    {
        data->pipeline_info.renderPass = rt->GetVkRenderPass();
        data->pipeline_info.subpass = 0;                   //subpass���ڻ���֪����ʲô�ã�������ʱд0����֪�����ú���Ľ�
    }

    if (vkCreateGraphicsPipelines(  attr->device,
                                    attr->pipeline_cache,
                                    1,&data->pipeline_info,
                                    nullptr,
                                    &graphicsPipeline) != VK_SUCCESS)
        return(nullptr);

    return(new Pipeline(attr->device,graphicsPipeline,data));
}

Pipeline *GPUDevice::CreatePipeline(const InlinePipeline &ip,const Material *mtl,const RenderTarget *rt)
{
    PipelineData *pd=GetPipelineData(ip);

    if(!pd)return(nullptr);

    return this->CreatePipeline(pd,mtl,rt);
}
VK_NAMESPACE_END
