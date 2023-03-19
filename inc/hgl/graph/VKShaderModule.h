#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/type/SortedSets.h>

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

    const bool                              IsVertex        ()const{return stage_create_info->stage==VK_SHADER_STAGE_VERTEX_BIT;}
    const bool                              IsTessControl   ()const{return stage_create_info->stage==VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;}
    const bool                              IsTessEval      ()const{return stage_create_info->stage==VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;}
    const bool                              IsGeometry      ()const{return stage_create_info->stage==VK_SHADER_STAGE_GEOMETRY_BIT;}
    const bool                              IsFragment      ()const{return stage_create_info->stage==VK_SHADER_STAGE_FRAGMENT_BIT;}
    const bool                              IsCompute       ()const{return stage_create_info->stage==VK_SHADER_STAGE_COMPUTE_BIT;}
    const bool                              IsTask          ()const{return stage_create_info->stage==VK_SHADER_STAGE_TASK_BIT_NV;}
    const bool                              IsMesh          ()const{return stage_create_info->stage==VK_SHADER_STAGE_MESH_BIT_NV;}

    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}

    operator VkShaderModule                                 ()const{return stage_create_info->module;}
};//class ShaderModule
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
