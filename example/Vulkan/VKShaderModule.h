#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
/**
 * Shader模块<br>
 * 该模块提供的是原始的shader数据和信息，不可被修改，只能通过ShaderModuleManage创建和删除
 */
class ShaderModule
{
    int shader_id;

    int ref_count;

private:

    VkPipelineShaderStageCreateInfo *stage_create_info;

    uint32_t attr_count;
    VkVertexInputBindingDescription *binding_list;
    VkVertexInputAttributeDescription *attribute_list;

private:

    Map<UTF8String,VkVertexInputAttributeDescription *> stage_input_locations;
    Map<UTF8String,int> ubo_list;

public:
    
    ShaderModule(int id,VkPipelineShaderStageCreateInfo *);
    ~ShaderModule();

    const int GetID()const{return shader_id;}

    const int IncRef(){return ++ref_count;}
    const int DecRef(){return --ref_count;}

public:

    const VkShaderStageFlagBits             GetStage        ()const{return stage_create_info->stage;}
    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}

    const int                               GetUBO          (const UTF8String &name)const
    {
        int binding;
        
        if(ubo_list.Get(name,binding))
            return binding;
        else 
            return -1;
    }

    const uint32_t                              GetAttrCount()const{return attr_count;}

    const int                                   GetLocation (const UTF8String &)const;
    const int                                   GetBinding  (const UTF8String &)const;

    const VkVertexInputBindingDescription *     GetDescList ()const{return binding_list;}
    const VkVertexInputAttributeDescription *   GetAttrList ()const{return attribute_list;}

    const VkVertexInputBindingDescription *     GetDesc     (const uint32_t index)const{return (index>=attr_count?nullptr:binding_list+index);}
    const VkVertexInputAttributeDescription *   GetAttr     (const uint32_t index)const{return (index>=attr_count?nullptr:attribute_list+index);}
};//class ShaderModule
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
