#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKVertexInputFormat.h>

namespace hgl::graph{

/**
 * Shader模块<br>
 * 该模块提供的是原始的shader数据和信息，不可被修改，只能通过ShaderModuleManage创建和删除
 */
class ShaderModule
{
    VkDevice device;
    int ref_count;
    uint64_t spv_content_hash;      ///< SPIRV 字节内容 hash（pipeline 缓存键的身份，见 D2）

private:

    VkPipelineShaderStageCreateInfo *stage_create_info;

public:

    ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci,uint64_t content_hash);
    virtual ~ShaderModule();

    const int IncRef(){return ++ref_count;}
    const int DecRef(){return --ref_count;}

public:

    const VkShaderStageFlagBits             GetStage        ()const{return stage_create_info->stage;}

    const bool                              IsFragment      ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Fragment;}
    const bool                              IsCompute       ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Compute;}
    const bool                              IsTask          ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Task;}
    const bool                              IsMesh          ()const{return stage_create_info->stage==(VkShaderStageFlagBits)ShaderStage::Mesh;}

    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}

    /// SPIRV 字节内容 hash——pipeline 缓存键的身份依据（禁止用 module 句柄值：
    /// 句柄在 module 销毁后可被新建模块复用，会让不同 shader 错误命中同一键）。
    const uint64_t                          GetSPVContentHash()const{return spv_content_hash;}

    operator VkShaderModule                                 ()const{return stage_create_info->module;}
};//class ShaderModule
}//namespace hgl::graph
