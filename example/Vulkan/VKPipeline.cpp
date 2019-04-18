#include"VKPipeline.h"
#include"VKShader.h"
#include"VKVertexInput.h"

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    vkDestroyPipeline(device,pipeline,nullptr);
}

PipelineCreater::PipelineCreater(VkDevice dev,uint w,uint h)
{
    device=dev;
    width=w;
    height=h;
    hgl_zero(pipelineInfo);
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent.width=width;
    scissor.extent.height=height;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    pipelineInfo.pViewportState = &viewportState;
}

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
    if(vi->GetCount()<=0)return(false);

    vertex_input=vi;

    vis_create_info=vertex_input->GetPipelineVertexInputStateCreateInfo();

    pipelineInfo.pVertexInputState=&vis_create_info;
    return(true);
}

bool PipelineCreater::Set(const VkPrimitiveTopology topology,bool restart)
{
    if(topology<VK_PRIMITIVE_TOPOLOGY_BEGIN_RANGE||topology>VK_PRIMITIVE_TOPOLOGY_END_RANGE)return(false);

    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = restart;

    pipelineInfo.pInputAssemblyState = &inputAssembly;
    return(true);
}

Pipeline *PipelineCreater::Create()
{
    //pipelineInfo.pRasterizationState = &rasterizer;
    //pipelineInfo.pMultisampleState = &multisampling;
    //pipelineInfo.pColorBlendState = &colorBlending;
    //pipelineInfo.layout = pipelineLayout;
    //pipelineInfo.renderPass = renderPass;
    //pipelineInfo.subpass = 0;
    //pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline graphicsPipeline;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) 
        return(nullptr);

    return(new Pipeline(device,graphicsPipeline));
}
VK_NAMESPACE_END
