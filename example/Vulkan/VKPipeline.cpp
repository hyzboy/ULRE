#include"VKPipeline.h"
#include"VKShader.h"
#include"VKVertexInput.h"

VK_NAMESPACE_BEGIN
bool PipelineCreater::Set(const Shader *s)
{
    if(!s)return(false);

    //未来这里需要增加是否有vs/fs的检测

    shader=s;

    pipelineInfo.stageCount=shader->GetCount();
    pipelineInfo.pStages=shader->GetStages();

    return(true);
}

bool PipelineCreater::Set(const VertexInput *vi)
{
    if(!vi)return(false);

    vertex_input=vi;

    vis_create_info=vertex_input->GetPipelineVertexInputStateCreateInfo();

    pipelineInfo.pVertexInputState=&vis_create_info;
    return(true);
}

Pipeline *PipelineCreater::Create()
{
    //pipelineInfo.pInputAssemblyState = &inputAssembly;
    //pipelineInfo.pViewportState = &viewportState;
    //pipelineInfo.pRasterizationState = &rasterizer;
    //pipelineInfo.pMultisampleState = &multisampling;
    //pipelineInfo.pColorBlendState = &colorBlending;
    //pipelineInfo.layout = pipelineLayout;
    //pipelineInfo.renderPass = renderPass;
    //pipelineInfo.subpass = 0;
    //pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    //VkPipeline graphicsPipeline;

    //if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
    //    throw std::runtime_error("failed to create graphics pipeline!");
    //}

    return(nullptr);
}
VK_NAMESPACE_END
