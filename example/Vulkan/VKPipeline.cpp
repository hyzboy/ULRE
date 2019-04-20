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
    cache=dev->GetPipelineCache();

    hgl_zero(pipelineInfo);
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    {
        vis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vis_create_info.pNext = nullptr;
        vis_create_info.flags = 0;

        vis_create_info.vertexBindingDescriptionCount = 0;
        vis_create_info.pVertexBindingDescriptions = nullptr;

        vis_create_info.vertexAttributeDescriptionCount = 0;
        vis_create_info.pVertexAttributeDescriptions = nullptr;

        pipelineInfo.pVertexInputState=&vis_create_info;
    }

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = extent;

    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    pipelineInfo.pViewportState = &viewportState;

    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext = nullptr;
    depthStencilState.flags = 0;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0;
    depthStencilState.maxDepthBounds = 0;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.back.compareMask = 0;
    depthStencilState.back.reference = 0;
    depthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.writeMask = 0;
    depthStencilState.front = depthStencilState.back;

    pipelineInfo.pDepthStencilState=&depthStencilState;

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.pNext = nullptr;
    rasterizer.flags = 0;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;     //顺时针
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0;
    rasterizer.depthBiasClamp = 0;
    rasterizer.depthBiasSlopeFactor = 0;
    rasterizer.lineWidth = 1.0f;

    pipelineInfo.pRasterizationState = &rasterizer;

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.pNext = nullptr;
    multisampling.flags = 0;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 0.0;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    pipelineInfo.pMultisampleState = &multisampling;

    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.flags = 0;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    pipelineInfo.pColorBlendState = &colorBlending;

    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = nullptr;
    dynamicState.flags = 0;
    dynamicState.pDynamicStates = dynamicStateEnables;
    dynamicState.dynamicStateCount = 0;
    dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStateEnables[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;

    pipelineInfo.pDynamicState=&dynamicState;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;
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

    return(true);
}

bool PipelineCreater::Set(const VkPrimitiveTopology topology,bool restart)
{
    if(topology<VK_PRIMITIVE_TOPOLOGY_BEGIN_RANGE||topology>VK_PRIMITIVE_TOPOLOGY_END_RANGE)
        if(topology!=PRIM_RECTANGLE)return(false);

    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.pNext = nullptr;
    inputAssembly.flags = 0;
    inputAssembly.topology = (topology==PRIM_RECTANGLE?VK_PRIMITIVE_TOPOLOGY_POINT_LIST:topology);
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

    if (vkCreateGraphicsPipelines(device, cache, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        return(nullptr);

    return(new Pipeline(device,graphicsPipeline));
}
VK_NAMESPACE_END
