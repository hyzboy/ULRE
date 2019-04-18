#include"VKPipeline.h"
#include"VKDevice.h"
#include"VKShader.h"
#include"VKVertexInput.h"
#include"VKRenderPass.h"

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    vkDestroyPipeline(device,pipeline,nullptr);
}

PipelineCreater::PipelineCreater(Device *dev)
{
    device=dev->GetDevice();
    extent=dev->GetExtent();

    hgl_zero(pipelineInfo);
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = extent;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    pipelineInfo.pViewportState = &viewportState;

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    pipelineInfo.pRasterizationState = &rasterizer;

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    pipelineInfo.pMultisampleState = &multisampling;

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;
    
    pipelineInfo.pColorBlendState = &colorBlending;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
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

bool PipelineCreater::Set(VkPipelineLayout pl)
{
    if(!pl)return(false);

    pipelineInfo.layout = pl;
    return(true);
}

bool PipelineCreater::Set(VkRenderPass rp,uint32_t subpass)
{
    if(!rp)return(false);

    pipelineInfo.renderPass = rp;
    pipelineInfo.subpass = subpass;

    return(true);
}

Pipeline *PipelineCreater::Create()
{
    VkPipeline graphicsPipeline;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) 
        return(nullptr);

    return(new Pipeline(device,graphicsPipeline));
}
VK_NAMESPACE_END
