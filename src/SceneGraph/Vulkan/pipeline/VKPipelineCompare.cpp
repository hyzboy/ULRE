#include<vulkan/vulkan.h>
#include<hgl/TypeFunc.h>
#include<hgl/math/MathConst.h>

namespace hgl::graph
{
    const bool Compare(const VkPipelineShaderStageCreateInfo &a,const VkPipelineShaderStageCreateInfo &b)
    {
        if(a.stage!=b.stage)
            return(false);

        if(a.module!=b.module)
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineVertexInputStateCreateInfo *a,const VkPipelineVertexInputStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->vertexBindingDescriptionCount!=b->vertexBindingDescriptionCount)
            return(false);

        if(a->vertexAttributeDescriptionCount!=b->vertexAttributeDescriptionCount)
            return(false);

        if(hgl_cmp(a->pVertexBindingDescriptions,b->pVertexBindingDescriptions,a->vertexBindingDescriptionCount))
            return(false);

        if(hgl_cmp(a->pVertexAttributeDescriptions,b->pVertexAttributeDescriptions,a->vertexAttributeDescriptionCount))
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineInputAssemblyStateCreateInfo *a,const VkPipelineInputAssemblyStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->topology!=b->topology)
            return(false);

        if(a->primitiveRestartEnable!=b->primitiveRestartEnable)
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineTessellationStateCreateInfo *a,const VkPipelineTessellationStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->patchControlPoints!=b->patchControlPoints)
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineViewportStateCreateInfo *a,const VkPipelineViewportStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->viewportCount!=b->viewportCount)
            return(false);

        if(a->scissorCount!=b->scissorCount)
            return(false);

        if(hgl_cmp(a->pViewports,b->pViewports,a->viewportCount))
            return(false);

        if(hgl_cmp(a->pScissors,b->pScissors,a->scissorCount))
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineRasterizationStateCreateInfo *a,const VkPipelineRasterizationStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->depthClampEnable!=b->depthClampEnable)
            return(false);
        if(a->rasterizerDiscardEnable!=b->rasterizerDiscardEnable)
            return(false);
        if(a->polygonMode!=b->polygonMode)
            return(false);
        if(a->cullMode!=b->cullMode)
            return(false);
        if(a->frontFace!=b->frontFace)
            return(false);
        if(a->depthBiasEnable!=b->depthBiasEnable)
            return(false);

        if(!IsNearlyEqual(a->depthBiasConstantFactor,b->depthBiasConstantFactor))
            return(false);
        if(!IsNearlyEqual(a->depthBiasClamp,b->depthBiasClamp))
            return(false);
        if(!IsNearlyEqual(a->depthBiasSlopeFactor,b->depthBiasSlopeFactor))
            return(false);
        if(!IsNearlyEqual(a->lineWidth,b->lineWidth))
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineMultisampleStateCreateInfo *a,const VkPipelineMultisampleStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->rasterizationSamples!=b->rasterizationSamples)
            return(false);
        if(a->sampleShadingEnable!=b->sampleShadingEnable)
            return(false);

        if(!IsNearlyEqual(a->minSampleShading,b->minSampleShading))
            return(false);

        if(a->pSampleMask!=b->pSampleMask)
            return(false);

//        if(hgl_cmp(a->pSampleMask,b->pSampleMask,))     //未支持sample mask，所以暂无法比较
//            return(false);

        if(a->alphaToCoverageEnable!=b->alphaToCoverageEnable)
            return(false);

        if(a->alphaToOneEnable!=b->alphaToOneEnable)
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineDepthStencilStateCreateInfo *a,const VkPipelineDepthStencilStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->depthTestEnable!=b->depthTestEnable)
            return(false);
        if(a->depthWriteEnable!=b->depthWriteEnable)
            return(false);
        if(a->depthCompareOp!=b->depthCompareOp)
            return(false);
        if(a->depthBoundsTestEnable!=b->depthBoundsTestEnable)
            return(false);
        if(a->stencilTestEnable!=b->stencilTestEnable)
            return(false);

        if(hgl_cmp(a->front,b->front))
            return(false);
        if(hgl_cmp(a->back,b->back))
            return(false);

        if(!IsNearlyEqual(a->minDepthBounds,b->minDepthBounds))
            return(false);
        if(!IsNearlyEqual(a->maxDepthBounds,b->maxDepthBounds))
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineColorBlendStateCreateInfo *a,const VkPipelineColorBlendStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);
        if(a->logicOpEnable!=b->logicOpEnable)
            return(false);
        if(a->logicOp!=b->logicOp)
            return(false);
        if(a->attachmentCount!=b->attachmentCount)
            return(false);
        if(hgl_cmp(a->pAttachments,b->pAttachments,a->attachmentCount))
            return(false);

        if(!IsNearlyEqualArray(a->blendConstants,b->blendConstants,4))
            return(false);

        return(true);
    }

    const bool Compare(const VkPipelineDynamicStateCreateInfo *a,const VkPipelineDynamicStateCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->dynamicStateCount!=b->dynamicStateCount)
            return(false);
        if(hgl_cmp(a->pDynamicStates,b->pDynamicStates,a->dynamicStateCount))
            return(false);

        return(true);
    }

    const bool Compare(const VkGraphicsPipelineCreateInfo *a,const VkGraphicsPipelineCreateInfo *b)
    {
        if(!a&&!b)return(true);
        if(!a||!b)return(false);

        if(a->stageCount!=b->stageCount)
            return(false);

        if(a->layout!=b->layout)
            return(false);

        if(a->renderPass!=b->renderPass)
            return(false);

        if(a->subpass!=b->subpass)
            return(false);

        if(a->basePipelineHandle!=b->basePipelineHandle)
            return(false);

        if(a->basePipelineIndex!=b->basePipelineIndex)
            return(false);

        for(uint32_t i=0;i<a->stageCount;i++)
            if(!Compare(a->pStages[i],b->pStages[i]))
                return(false);

        if(!Compare(a->pVertexInputState,b->pVertexInputState))
            return(false);

        if(!Compare(a->pInputAssemblyState,b->pInputAssemblyState))
            return(false);

        if(!Compare(a->pTessellationState,b->pTessellationState))
            return(false);

        //if(!Compare(a->pViewportState,b->pViewportState))return(false);

        if(!Compare(a->pRasterizationState,b->pRasterizationState))
            return(false);

        if(!Compare(a->pMultisampleState,b->pMultisampleState))
            return(false);

        if(!Compare(a->pDepthStencilState,b->pDepthStencilState))
            return(false);

        if(!Compare(a->pColorBlendState,b->pColorBlendState))
            return(false);

        //if(!Compare(a->pDynamicState,b->pDynamicState))return(false);

        return(true);
    }
}//namespace hgl::graph
