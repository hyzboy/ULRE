#ifndef HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_MANAGER_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_MANAGER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKShaderDescriptorSet.h>

VK_NAMESPACE_BEGIN
class MaterialDescriptorManager
{
    UTF8String mtl_name;

    Map<AnsiString,int> binding_map[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

private:

    VkDescriptorSetLayoutBinding *all_dslb;

    DescriptorSetLayoutCreateInfo dsl_ci[DESCRIPTOR_SET_TYPE_COUNT];

public:

    MaterialDescriptorManager(const UTF8String &,ShaderDescriptor *,const uint);
    MaterialDescriptorManager(const UTF8String &,const ShaderDescriptorSetArray &);
    ~MaterialDescriptorManager();

    const UTF8String &GetMaterialName()const{return mtl_name;}

    const int GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const;

    const int GetUBO            (const AnsiString &name,bool dynamic)const{return GetBinding(dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,name);}
    const int GetSSBO           (const AnsiString &name,bool dynamic)const{return GetBinding(dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,name);}
    const int GetImageSampler   (const AnsiString &name             )const{return GetBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,name);}
    const int GetInputAttachment(const AnsiString &name             )const{return GetBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,name);}

    const DescriptorSetLayoutCreateInfo *GetDSLCI(const DescriptorSetType &type)const{return dsl_ci+size_t(type);}

    const bool hasSet(const DescriptorSetType &type)const{return dsl_ci[size_t(type)].bindingCount>0;}
//!sd_list_by_set_type[size_t(type)].IsEmpty();}
};//class MaterialDescriptorManager
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_MANAGER_INCLUDE