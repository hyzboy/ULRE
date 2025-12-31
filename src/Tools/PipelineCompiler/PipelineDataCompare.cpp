#include<hgl/graph/VKPipelineData.h>


VK_NAMESPACE_BEGIN

namespace
{
    #define COMPARE_VALUE(value)    if(v1->value!=v2->value)return(false);
    #define COMPARE_FLOAT(value)    if(!IsNearlyEqual(v1->value,v2->value))return(false);
    #define COMPARE_FLOAT_ARRAY(value,count)    if(!IsNearlyEqualArray(v1->value,v2->value,count))return(false);
    #define COMPARE_VALUE_ARRAY(value,count)    if(hgl_cmp(v1->value,v2->value,count))return(false);
    #define COMPARE_TYPE(value)     if(hgl_cmp(v1->value,v2->value))return(false);

    #define COMPARE_BEGIN(value) bool Compare(const VkPipeline##value##StateCreateInfo *v1,const VkPipeline##value##StateCreateInfo *v2)   \
    {   \
        if(!v1||!v2)return(false);

    #define COMPARE_END return(true);}

    COMPARE_BEGIN(Tessellation)
        COMPARE_VALUE(patchControlPoints)
    COMPARE_END

    COMPARE_BEGIN(Rasterization)
        COMPARE_VALUE(depthClampEnable)
        COMPARE_VALUE(rasterizerDiscardEnable)
        COMPARE_VALUE(polygonMode)
        COMPARE_VALUE(cullMode)
        COMPARE_VALUE(frontFace)
        COMPARE_VALUE(depthBiasEnable)
        COMPARE_FLOAT(depthBiasConstantFactor)
        COMPARE_FLOAT(depthBiasClamp)
        COMPARE_FLOAT(depthBiasSlopeFactor)
        COMPARE_FLOAT(lineWidth)
    COMPARE_END

    COMPARE_BEGIN(Multisample)
        COMPARE_VALUE(rasterizationSamples)
        COMPARE_VALUE(sampleShadingEnable)
        COMPARE_FLOAT(minSampleShading)
        COMPARE_VALUE(alphaToCoverageEnable)
        COMPARE_VALUE(alphaToOneEnable)
        //COMPARE_VALUE_ARRAY(pSampleMask,0)
    COMPARE_END

    COMPARE_BEGIN(DepthStencil)
        COMPARE_VALUE(depthTestEnable)
        COMPARE_VALUE(depthWriteEnable)
        COMPARE_VALUE(depthCompareOp)
        COMPARE_VALUE(depthBoundsTestEnable)
        COMPARE_VALUE(stencilTestEnable)
        COMPARE_TYPE(front)
        COMPARE_TYPE(back)
        COMPARE_FLOAT(minDepthBounds)
        COMPARE_FLOAT(maxDepthBounds)
    COMPARE_END

    COMPARE_BEGIN(ColorBlend)
        COMPARE_VALUE(logicOpEnable)
        COMPARE_VALUE(logicOp)
        COMPARE_VALUE(attachmentCount)
        COMPARE_VALUE_ARRAY(pAttachments,v1->attachmentCount)
        COMPARE_FLOAT_ARRAY(blendConstants,4)
    COMPARE_END
}//namespace

bool Compare(const PipelineData *pd1,const PipelineData *pd2)
{
    if(!pd1||!pd2)return(false);

    if(!Compare(pd1->tessellation,pd2->tessellation))return(false);
    if(!Compare(pd1->rasterization,pd2->rasterization))return(false);
    if(!Compare(pd1->multi_sample,pd2->multi_sample))return(false);
    if(!Compare(pd1->depth_stencil,pd2->depth_stencil))return(false);
    if(!Compare(pd1->color_blend,pd2->color_blend))return(false);
    if(!Compare(pd1->color_blend,pd2->color_blend))return(false);

    if(pd1->alpha_test!=pd2->alpha_test)return(false);
    if(pd1->alpha_blend!=pd2->alpha_blend)return(false);

    return(true);
}

VK_NAMESPACE_END
