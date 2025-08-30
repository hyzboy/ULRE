#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKVertexInputLayout.h>

VK_NAMESPACE_BEGIN

/**
 * Shader模块<br>
 * 该模块提供的是原始的shader数据和信息，不可被修改，只能通过ShaderModuleManage创建和删除
 */
class ShaderModule
{
    VkDevice device;
    int ref_count;

private:

    VkPipelineShaderStageCreateInfo *stage_create_info;

public:

    ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci);
    virtual ~ShaderModule();

    const int IncRef(){return ++ref_count;}
    const int DecRef(){return --ref_count;}

public:

    const VkShaderStageFlagBits             GetStage        ()const{return stage_create_info->stage;}

    const bool                              IsVertex        ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Vertex;}
    const bool                              IsTessControl   ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::TessellationControl;}
    const bool                              IsTessEval      ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::TessellationEvaluation;}
    const bool                              IsGeometry      ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Geometry;}
    const bool                              IsFragment      ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Fragment;}
    const bool                              IsCompute       ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Compute;}
    const bool                              IsTask          ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Task;}
    const bool                              IsMesh          ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Mesh;}

    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}

    operator VkShaderModule                                 ()const{return stage_create_info->module;}
};//class ShaderModule
VK_NAMESPACE_END
