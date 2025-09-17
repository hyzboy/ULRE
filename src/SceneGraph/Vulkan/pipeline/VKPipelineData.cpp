#include<hgl/graph/pipeline/VKPipelineData.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKDeviceAttribute.h>

VK_NAMESPACE_BEGIN
void SetDefault(VkPipelineColorBlendAttachmentState *cba)
{    
    cba->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba->blendEnable = VK_FALSE;
    cba->alphaBlendOp = VK_BLEND_OP_ADD;
    cba->colorBlendOp = VK_BLEND_OP_ADD;
    cba->srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba->dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba->srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
}

PipelineData::PipelineData(const PipelineData *pd)
{
    file_data=nullptr;

    hgl_zero(pipeline_info);

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    vertex_input_binding_description=nullptr;
    vertex_input_attribute_description=nullptr;

    viewport=pd->viewport;
    scissor=pd->scissor;

    viewport_state=pd->viewport_state;

    viewport_state.pScissors=&scissor;
    viewport_state.pViewports=&viewport;

    pipeline_info.pViewportState     = &viewport_state;

#define PIPELINE_STRUCT_NEW_COPY(pname,name)  pipeline_info.pname=name=hgl_new_copy(pd->name);

    PIPELINE_STRUCT_NEW_COPY(pTessellationState,tessellation);
    PIPELINE_STRUCT_NEW_COPY(pRasterizationState,rasterization);

    sample_mask=nullptr;
    PIPELINE_STRUCT_NEW_COPY(pMultisampleState,multi_sample);

    PIPELINE_STRUCT_NEW_COPY(pDepthStencilState,depth_stencil);
    
#undef PIPELINE_STRUCT_COPY

    InitColorBlend(pd->color_blend->attachmentCount,pd->color_blend_attachments);

    hgl_cpy(dynamic_state_enables,pd->dynamic_state_enables);
    hgl_cpy(dynamic_state,pd->dynamic_state);

    dynamic_state.pDynamicStates=dynamic_state_enables;
    pipeline_info.pDynamicState =&dynamic_state;

    alpha_test=pd->alpha_test;
    alpha_blend=pd->alpha_blend;

    {
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;
    }
}

PipelineData::PipelineData(const uint32_t color_attachment_count)
{
    file_data=nullptr;

    hgl_zero(pipeline_info);
    //hgl_zero(vis_create_info);
    //hgl_zero(input_assembly);
    //hgl_zero(tessellation);
    //hgl_zero(viewport);
    //hgl_zero(scissor);
    //hgl_zero(viewport_state);
    //hgl_zero(rasterization);    
    //hgl_zero(sample_mask);
    //hgl_zero(multi_sample);
    //hgl_zero(depth_stencil);    
    //hgl_zero(color_blend);
    //hgl_zero(dynamic_state_enables);
    //hgl_zero(dynamic_state);

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    vertex_input_binding_description=nullptr;
    vertex_input_attribute_description=nullptr;

    InitViewportState();
    InitDynamicState();
    
    tessellation=new VkPipelineTessellationStateCreateInfo;
    tessellation->sType=VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation->pNext=nullptr;
    tessellation->flags=0;
    tessellation->patchControlPoints=0;

    pipeline_info.pTessellationState=tessellation;
    
    rasterization=new VkPipelineRasterizationStateCreateInfo;
    rasterization->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization->pNext = nullptr;
    rasterization->flags = 0;
    rasterization->depthClampEnable = VK_FALSE;
    rasterization->rasterizerDiscardEnable = VK_FALSE;
    rasterization->polygonMode = VK_POLYGON_MODE_FILL;
    rasterization->cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization->frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization->depthBiasEnable = VK_FALSE;
    rasterization->depthBiasConstantFactor = 0;
    rasterization->depthBiasClamp = 0;
    rasterization->depthBiasSlopeFactor = 0;
    rasterization->lineWidth = 1.0f;

    pipeline_info.pRasterizationState=rasterization;
    
    multi_sample=new VkPipelineMultisampleStateCreateInfo;
    multi_sample->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multi_sample->pNext = nullptr;
    multi_sample->flags = 0;
    multi_sample->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multi_sample->sampleShadingEnable = VK_FALSE;
    multi_sample->minSampleShading = 0.0;
    multi_sample->pSampleMask = nullptr;
    multi_sample->alphaToCoverageEnable = VK_FALSE;
    multi_sample->alphaToOneEnable = VK_FALSE;
    
    sample_mask=nullptr;

    pipeline_info.pMultisampleState=multi_sample;
    
    depth_stencil=new VkPipelineDepthStencilStateCreateInfo;
    depth_stencil->sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil->pNext = nullptr;
    depth_stencil->flags = 0;
    depth_stencil->depthTestEnable = VK_TRUE;
    depth_stencil->depthWriteEnable = VK_TRUE;
    depth_stencil->depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil->depthBoundsTestEnable = VK_FALSE;
    depth_stencil->minDepthBounds = 0;
    depth_stencil->maxDepthBounds = 0;
    depth_stencil->stencilTestEnable = VK_FALSE;
    depth_stencil->back.failOp = VK_STENCIL_OP_KEEP;
    depth_stencil->back.passOp = VK_STENCIL_OP_KEEP;
    depth_stencil->back.compareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil->back.compareMask = 0;
    depth_stencil->back.reference = 0;
    depth_stencil->back.depthFailOp = VK_STENCIL_OP_KEEP;
    depth_stencil->back.writeMask = 0;
    depth_stencil->front = depth_stencil->back;
    depth_stencil->front.compareOp=VK_COMPARE_OP_NEVER;

    pipeline_info.pDepthStencilState=depth_stencil;

    //这个需要和subpass中的color attachment数量相等，所以添加多份
    InitColorBlend(color_attachment_count);

    alpha_test=0;
    alpha_blend=false;

    {
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.basePipelineIndex = -1;
    }
}

void PipelineData::InitColorBlend(const uint32_t color_attachment_count,const VkPipelineColorBlendAttachmentState *pcbas)
{
    color_blend_attachments=hgl_align_malloc<VkPipelineColorBlendAttachmentState>(color_attachment_count);

    if(pcbas)
        hgl_cpy(color_blend_attachments,pcbas,color_attachment_count);
    else
    {
        SetDefault(color_blend_attachments);

        hgl_set(color_blend_attachments+1,color_blend_attachments,color_attachment_count-1);
    }

    color_blend=new VkPipelineColorBlendStateCreateInfo;
    color_blend->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend->pNext = nullptr;
    color_blend->flags = 0;
    color_blend->logicOpEnable = VK_FALSE;
    color_blend->logicOp = VK_LOGIC_OP_CLEAR;
    color_blend->attachmentCount = color_attachment_count;
    color_blend->pAttachments = color_blend_attachments;
    color_blend->blendConstants[0] = 0.0f;
    color_blend->blendConstants[1] = 0.0f;
    color_blend->blendConstants[2] = 0.0f;
    color_blend->blendConstants[3] = 0.0f;

    pipeline_info.pColorBlendState=color_blend;
}

void PipelineData::SetColorAttachments(const uint32_t count)
{
    if(!color_blend_attachments)
    {
        color_blend->attachmentCount=0;
        color_blend_attachments=hgl_align_malloc<VkPipelineColorBlendAttachmentState>(count);
        SetDefault(color_blend_attachments);
    }
    else
    {
        if(color_blend->attachmentCount==count)return;

        color_blend_attachments=hgl_align_realloc<VkPipelineColorBlendAttachmentState>(color_blend_attachments,count);
    }

    if(count>color_blend->attachmentCount)
    {        
        VkPipelineColorBlendAttachmentState *cba=color_blend_attachments+color_blend->attachmentCount;

        for(uint32_t i=color_blend->attachmentCount;i<count;i++)
        {
            memcpy(cba,color_blend_attachments,sizeof(VkPipelineColorBlendAttachmentState));
            ++cba;
        }
    }

    color_blend->attachmentCount=count;
    color_blend->pAttachments = color_blend_attachments;
}

PipelineData::PipelineData()
{
    file_data=nullptr;

    hgl_zero(pipeline_info);
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    vertex_input_attribute_description=nullptr;
    vertex_input_binding_description=nullptr;

    sample_mask=nullptr;
   
    tessellation              =hgl_zero_new<VkPipelineTessellationStateCreateInfo>();
    rasterization             =hgl_zero_new<VkPipelineRasterizationStateCreateInfo>();
    multi_sample              =hgl_zero_new<VkPipelineMultisampleStateCreateInfo>();
    sample_mask               =hgl_zero_new<VkSampleMask>(VK_NAMESPACE::MAX_SAMPLE_MASK_COUNT);
    multi_sample->pSampleMask =nullptr;
    
    depth_stencil             =hgl_zero_new<VkPipelineDepthStencilStateCreateInfo>();
    color_blend               =hgl_zero_new<VkPipelineColorBlendStateCreateInfo>();

    InitColorBlend(32);//暂时不可能MRT输出32个，就这样了
    
    pipeline_info.pTessellationState =tessellation;
    pipeline_info.pRasterizationState=rasterization;
    pipeline_info.pMultisampleState  =multi_sample;
    pipeline_info.pDepthStencilState =depth_stencil;
    pipeline_info.pColorBlendState   =color_blend;
    
    alpha_test=0;
    alpha_blend=false;

    InitViewportState();
    InitDynamicState();
}

void PipelineData::InitShaderStage(const ShaderStageCreateInfoList &ssl)
{
    pipeline_info.stageCount = ssl.GetCount();
    pipeline_info.pStages = ssl.GetData();
}

void PipelineData::InitVertexInputState(const VIL *vil)
{
    vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state.pNext = nullptr;
    vertex_input_state.flags = 0;

    vertex_input_state.vertexBindingDescriptionCount   = 
    vertex_input_state.vertexAttributeDescriptionCount = vil->GetVertexAttribCount();

    vertex_input_binding_description    =vil->NewBindListCopy();
    vertex_input_attribute_description  =vil->NewAttrListCopy();

    vertex_input_state.pVertexBindingDescriptions      = vertex_input_binding_description;
    vertex_input_state.pVertexAttributeDescriptions    = vertex_input_attribute_description;
    
    pipeline_info.pVertexInputState  = &vertex_input_state;
}
namespace
{
    struct ComplexPrimitiveTopologyToOrigin
    {
        VkPrimitiveTopology vk_prim;
        PrimitiveType prim;
    };

    constexpr const ComplexPrimitiveTopologyToOrigin ComplexPrimitiveTopologyToOriginList[]=
    {
        {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,  PrimitiveType::SolidRectangles},
        {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,  PrimitiveType::SolidCircles},
        {VK_PRIMITIVE_TOPOLOGY_LINE_LIST,   PrimitiveType::WireRectangles},
        {VK_PRIMITIVE_TOPOLOGY_LINE_LIST,   PrimitiveType::WireCircles},
        {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,  PrimitiveType::Billboard}
    };

    const VkPrimitiveTopology GetVkPrimitive(const PrimitiveType &p)
    {
        if(RangeCheck(p))
            return VkPrimitiveTopology(p);

        for(const ComplexPrimitiveTopologyToOrigin &cptol:ComplexPrimitiveTopologyToOriginList)
            if(cptol.prim==p)
                return cptol.vk_prim;

        return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
    }
}//namespace

bool PipelineData::SetPrim(const PrimitiveType topology,bool prim_restart)
{
    VkPrimitiveTopology prim=GetVkPrimitive(topology);

    if(prim==VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
        return(false);

    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = nullptr;
    input_assembly.flags = 0;
    input_assembly.topology = prim;
    input_assembly.primitiveRestartEnable = prim_restart;

    pipeline_info.pInputAssemblyState = &input_assembly;
    return(true);
}
    
void PipelineData::InitViewportState()
{
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 0.0f;
    viewport.height = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    scissor.offset = {0, 0};
    scissor.extent = {0, 0};

    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = nullptr;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;
    
    pipeline_info.pViewportState     = &viewport_state;
}

void PipelineData::InitDynamicState()
{
    hgl_zero(dynamic_state_enables);

    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.pNext = nullptr;
    dynamic_state.flags = 0;
    dynamic_state.pDynamicStates = dynamic_state_enables;
    dynamic_state.dynamicStateCount = 0;

    ArrayWriter dynamicStates(&dynamic_state.dynamicStateCount,dynamic_state_enables);

    dynamicStates
        << VK_DYNAMIC_STATE_VIEWPORT
        << VK_DYNAMIC_STATE_SCISSOR;
    //    << VK_DYNAMIC_STATE_LINE_WIDTH;

    //if(dev_attr)
    //{

    //    << VK_DYNAMIC_STATE_CULL_MODE
    //        << VK_DYNAMIC_STATE_FRONT_FACE

    //        << VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY

    //        << VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE
    //        << VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE
    //        << VK_DYNAMIC_STATE_DEPTH_COMPARE_OP

    //        << VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE
    //        << VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE;
    //}

                    //<< VK_DYNAMIC_STATE_LINE_STIPPLE;

    pipeline_info.pDynamicState      = &dynamic_state;
}

void PipelineData::AddDynamicState(VkDynamicState ds)
{
    for(uint32_t i = 0;i < dynamic_state.dynamicStateCount;i++)
        if(dynamic_state_enables[i] == ds)
            return;

    dynamic_state_enables[dynamic_state.dynamicStateCount] = ds;
    ++dynamic_state.dynamicStateCount;
}

bool PipelineData::SetColorWriteMask(uint index,bool r,bool g,bool b,bool a)
{
    if(index>=color_blend->attachmentCount)
        return(false);

    VkPipelineColorBlendAttachmentState *cba=color_blend_attachments+index;

    cba->colorWriteMask=0;

    if(r)cba->colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;
    if(r)cba->colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;
    if(g)cba->colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;
    if(a)cba->colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;

    return(true);
}

bool PipelineData::OpenBlend(uint index)
{
    if(index>=color_blend->attachmentCount)
        return(false);

    color_blend_attachments[index].blendEnable=true;

    return(true);
}

bool PipelineData::CloseBlend(uint index)
{
    if(index>=color_blend->attachmentCount)
        return(false);

    color_blend_attachments[index].blendEnable=false;

    return(true);
}

bool PipelineData::SetColorBlend(uint index,VkBlendOp op,VkBlendFactor src,VkBlendFactor dst)
{
    if(index>=color_blend->attachmentCount)
        return(false);
        
    VkPipelineColorBlendAttachmentState *cba=color_blend_attachments+index;

    cba->colorBlendOp=op;
    cba->srcColorBlendFactor=src;
    cba->dstColorBlendFactor=dst;

    return(true);
}

bool PipelineData::SetAlphaBlend(uint index,VkBlendOp op,VkBlendFactor src,VkBlendFactor dst)
{
    if(index>=color_blend->attachmentCount)
        return(false);
        
    VkPipelineColorBlendAttachmentState *cba=color_blend_attachments+index;

    cba->colorBlendOp=op;
    cba->srcAlphaBlendFactor=src;
    cba->dstAlphaBlendFactor=dst;

    return(true);
}
VK_NAMESPACE_END
