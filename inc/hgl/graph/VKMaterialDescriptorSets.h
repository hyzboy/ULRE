#ifndef HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

struct ShaderDescriptor
{
    char name[128];

    VkDescriptorType desc_type;
    DescriptorSetType set_type;
    uint8 set;
    uint8 binding;
    uint32 stage_flag;
};

using ShaderDescriptorList=List<ShaderDescriptor *>;

class MaterialDescriptorSets
{
    UTF8String mtl_name;

    ShaderDescriptor *sd_list;
    uint sd_count;

    ShaderDescriptorList sd_list_by_desc_type[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
    ShaderDescriptorList sd_list_by_set_type[size_t(DescriptorSetType::RANGE_SIZE)];

//    Map<AnsiString,ShaderDescriptor *> sd_by_name;
    Map<AnsiString,int> binding_map[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

//    int *binding_list[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

private:

    DescriptorSetLayoutCreateInfo dsl_ci[size_t(DescriptorSetType::RANGE_SIZE)];

public:

    MaterialDescriptorSets(const UTF8String &,ShaderDescriptor *,const uint);
    ~MaterialDescriptorSets();

    const UTF8String &GetMaterialName()const{return mtl_name;}

    const int GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const;

    const int GetUBO            (const AnsiString &name,bool dynamic)const{return GetBinding(dynamic?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,name);}
    const int GetSSBO           (const AnsiString &name,bool dynamic)const{return GetBinding(dynamic?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,name);}
    const int GetImageSampler   (const AnsiString &name             )const{return GetBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,name);}
    const int GetInputAttachment(const AnsiString &name             )const{return GetBinding(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,name);}

    const DescriptorSetLayoutCreateInfo *GetDSLCI(const DescriptorSetType &type)const{return dsl_ci+size_t(type);}

    const ShaderDescriptorList &GetDescriptorList(const DescriptorSetType &type)const{return sd_list_by_set_type[size_t(type)];}

    const bool hasSet(const DescriptorSetType &type)const{return !sd_list_by_set_type[size_t(type)].IsEmpty();}
};//class MaterialDescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE
