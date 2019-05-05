#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include"VK.h"
#include<hgl/type/Map.h>
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
class Device;
class ShaderModule;
class VertexShaderModule;
class DescriptorSets;
class DescriptorSetLayoutCreater;
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
    Device *device;
    ShaderModuleMap *shader_maps;
    VertexShaderModule *vertex_sm;
    List<VkPipelineShaderStageCreateInfo> *shader_stage_list;

    DescriptorSetLayoutCreater *dsl_creater;

    VertexAttributeBinding *vab;

public:

    Material(Device *dev,ShaderModuleMap *smm,List<VkPipelineShaderStageCreateInfo> *,DescriptorSetLayoutCreater *dslc);
    ~Material();

    const int GetUBOBinding(const UTF8String &)const;
    const int GetVBOBinding(const UTF8String &)const;

    const uint32_t                          GetStageCount       ()const{return shader_stage_list->GetCount();}
    const VkPipelineShaderStageCreateInfo * GetStages           ()const{return shader_stage_list->GetData();}

    const VkPipelineLayout                  GetPipelineLayout   ()const;
    DescriptorSets *                        CreateDescriptorSets()const;

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;

    Renderable *CreateRenderable();
};//class Material
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
