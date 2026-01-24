#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/type/ValueArray.h>
#include<hgl/type/String.h>
#include<cmath>

namespace hgl::io
{
    class DataOutputStream;
}//namespace hgl::io

VK_NAMESPACE_BEGIN

struct VulkanDevAttr;

const size_t MAX_SAMPLE_MASK_COUNT=VK_SAMPLE_COUNT_64_BIT;

const size_t VK_DYNAMIC_STATE_RANGE_SIZE=40;

using ShaderStageCreateInfoList=ValueArray<VkPipelineShaderStageCreateInfo>;

const bool Compare(const VkGraphicsPipelineCreateInfo *,const VkGraphicsPipelineCreateInfo *);

struct PipelineData
{
private:

    uchar *file_data;

public:

    VkGraphicsPipelineCreateInfo                pipeline_info;

    VkVertexInputBindingDescription *           vertex_input_binding_description;
    VkVertexInputAttributeDescription *         vertex_input_attribute_description;

    VkPipelineVertexInputStateCreateInfo        vertex_input_state;

    VkPipelineInputAssemblyStateCreateInfo      input_assembly;
    VkPipelineTessellationStateCreateInfo *     tessellation;

    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo           viewport_state;

    void InitViewportState();

    VkPipelineRasterizationStateCreateInfo *    rasterization;

    VkSampleMask *                              sample_mask;
    VkPipelineMultisampleStateCreateInfo *      multi_sample;

    VkPipelineDepthStencilStateCreateInfo *     depth_stencil;

    VkPipelineColorBlendAttachmentState *       color_blend_attachments;
    VkPipelineColorBlendStateCreateInfo *       color_blend;

    void InitColorBlend(const uint32_t,const VkPipelineColorBlendAttachmentState *pcbas=nullptr);

    VkDynamicState                              dynamic_state_enables[VK_DYNAMIC_STATE_RANGE_SIZE];
    VkPipelineDynamicStateCreateInfo            dynamic_state;

    float alpha_test;
    bool alpha_blend;

public:

    PipelineData(const PipelineData &)=delete;
    PipelineData(const PipelineData *);
    PipelineData(const uint32_t color_attachment_count);
    PipelineData();
    ~PipelineData();

    void InitShaderStage(const ShaderStageCreateInfoList &);
    void InitVertexInputState(const VIL *);
    void InitDynamicState();

    void AddDynamicState(VkDynamicState);

public:

    bool SetPrim(const PrimitiveType prim,bool prim_restart=false);

    void SetViewport(       float x,float y,float w,float h){viewport.x=x;viewport.y=y;viewport.width=w;viewport.height=h;}
    void SetDepthRange(     float min_depth,float max_depth){viewport.minDepth=min_depth;viewport.maxDepth=max_depth;}
    void SetScissor(        float l,float t,float w,float h){scissor.offset.x=l;scissor.offset.y=t;scissor.extent.width=w;scissor.extent.height=h;}

    void SetAlphaTest(      const float at)                 {alpha_test=at;}

    void SetDepthTest(      bool                dt)         {depth_stencil->depthTestEnable=dt;}
    void SetDepthWrite(     bool                dw)         {depth_stencil->depthWriteEnable=dw;}
    void SetDepthCompareOp( VkCompareOp         op)         {depth_stencil->depthCompareOp=op;}
    void SetDepthBoundsTest(bool                dbt)        {depth_stencil->depthBoundsTestEnable=dbt;}
    void SetDepthBounds(    float               min_depth,
                            float               max_depth)  {depth_stencil->depthBoundsTestEnable=VK_TRUE;
                                                             depth_stencil->minDepthBounds=min_depth;
                                                             depth_stencil->maxDepthBounds=max_depth;}
    void SetStencilTest(    bool                st)         {depth_stencil->stencilTestEnable=st;}

    void SetDepthClamp(     bool                dc)         {rasterization->depthClampEnable=dc;}
    void SetDiscard(        bool                discard)    {rasterization->rasterizerDiscardEnable=discard;}
    void SetPolygonMode(    VkPolygonMode       pm)         {rasterization->polygonMode =pm;}
    void SetCullMode(       VkCullModeFlagBits  cm)         {rasterization->cullMode    =cm;}
    void CloseCullFace()                                    {rasterization->cullMode    =VK_CULL_MODE_NONE;}
    void SetFrontFace(      VkFrontFace         ff)         {rasterization->frontFace   =ff;}
    void SetDepthBias(      float               ConstantFactor,
                            float               Clamp,
                            float               SlopeFactor)
    {
        rasterization->depthBiasEnable          =VK_TRUE;
        rasterization->depthBiasConstantFactor  =ConstantFactor;
        rasterization->depthBiasClamp           =Clamp;
        rasterization->depthBiasSlopeFactor     =SlopeFactor;
    }
    void DisableDepthBias()                                 {rasterization->depthBiasEnable=VK_FALSE;}
    void SetLineWidth(      float               line_width) {rasterization->lineWidth   =line_width;}

    void SetSamleCount(     VkSampleCountFlagBits sc)
    {
        multi_sample->sampleShadingEnable=(sc==VK_SAMPLE_COUNT_1_BIT?VK_FALSE:VK_TRUE);
        multi_sample->rasterizationSamples=sc;
    }

    bool SetColorWriteMask(uint index,bool r,bool g,bool b,bool a);
    bool OpenBlend(uint index);
    bool CloseBlend(uint index);
    bool SetColorBlend(uint index,VkBlendOp,VkBlendFactor,VkBlendFactor);
    bool SetAlphaBlend(uint index,VkBlendOp,VkBlendFactor,VkBlendFactor);

    void SetLogicOp(VkLogicOp logic_op) {color_blend->logicOpEnable=VK_TRUE;color_blend->logicOp=logic_op;}
    void DisableLogicOp()               {color_blend->logicOpEnable=VK_FALSE;}

    void SetBlendConstans(float r,float g,float b,float a)
    {
        color_blend->blendConstants[0] = r;
        color_blend->blendConstants[1] = g;
        color_blend->blendConstants[2] = b;
        color_blend->blendConstants[3] = a;
    }

    void SetBlendConstans(float *blend_constans)        {mem_copy(color_blend->blendConstants,blend_constans,4);}

    void SetColorAttachments(const uint32_t);

public:

    bool SaveToStream(io::DataOutputStream *dos)const;
    bool LoadFromMemory(uchar *,uint);
};//struct PipelineData

/**
 * 根据文件名获取PipelineData
 * @param filename 文件名(注意不包含扩展名)
 */
const PipelineData *GetPipelineData(const OSString &filename);
VK_NAMESPACE_END
