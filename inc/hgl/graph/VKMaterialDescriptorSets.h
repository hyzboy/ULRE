#ifndef HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

struct ShaderDescriptor
{
    char name[128];

    VkDescriptorType desc_type;
    DescriptorSetType set_type;
    uint32_t set;
    uint32_t binding;
    uint32_t stage_flag;
};

using ShaderDescriptorList=List<ShaderDescriptor *>;

class MaterialDescriptorSets
{
    ShaderDescriptor *sd_list;
    uint sd_count;

    ShaderDescriptorList descriptor_list[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

    Map<AnsiString,ShaderDescriptor *> sd_by_name;
    Map<AnsiString,int> binding_map[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

    int *binding_list[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

private:

    DescriptorSetLayoutCreateInfo sds[size_t(DescriptorSetType::RANGE_SIZE)];

public:

    MaterialDescriptorSets(ShaderDescriptor *,const uint);
    ~MaterialDescriptorSets();

    const int GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const;

    const int GetUBO    (const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          name);}
    const int GetSSBO   (const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          name);}
    const int GetSampler(const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  name);}

    const DescriptorSetLayoutCreateInfo *GetBinding(const DescriptorSetType &type)const{return sds+size_t(type);}
};//class MaterialDescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE
