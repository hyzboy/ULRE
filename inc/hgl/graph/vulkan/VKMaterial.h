#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include"VK.h"
#include<hgl/type/Map.h>
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
class Device;
class ShaderModule;
class VertexShaderModule;
class DescriptorSetLayoutCreater;
class DescriptorSetLayout;
class VertexAttributeBinding;
class VertexBuffer;
class Renderable;
class PipelineLayout;

using ShaderModuleMap=hgl::Map<VkShaderStageFlagBits,const ShaderModule *>;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    VkDevice device;
    ShaderModuleMap *shader_maps;
    VertexShaderModule *vertex_sm;
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list;
    DescriptorSetLayoutCreater *dsl_creater;
    DescriptorSetLayout *desc_set_layout;
    VertexAttributeBinding *vab;

public:

    Material(Device *dev,ShaderModuleMap *smm,VertexShaderModule *vsm,List<VkPipelineShaderStageCreateInfo> *,DescriptorSetLayoutCreater *dslc,DescriptorSetLayout *dsl,VertexAttributeBinding *v);
    ~Material();

    const int GetUBOBinding(const UTF8String &)const;
    const int GetVBOBinding(const UTF8String &)const;

    const uint32_t                          GetStageCount       ()const{return shader_stage_list->GetCount();}
    const VkPipelineShaderStageCreateInfo * GetStages           ()const{return shader_stage_list->GetData();}

    const VkPipelineLayout                  GetPipelineLayout       ()const;
    const uint32_t                          GetDescriptorSetCount   ()const;
    const VkDescriptorSet *                 GetDescriptorSets       ()const;

    bool UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info);
    bool UpdateUBO(const UTF8String &name,const VkDescriptorBufferInfo *buf_info)
    {
        if(name.IsEmpty()||!buf_info)
            return(false);

        return UpdateUBO(GetUBOBinding(name),buf_info);
    }

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;

    Renderable *CreateRenderable();
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
