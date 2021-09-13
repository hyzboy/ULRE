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

    struct ShaderDescriptorSet
    {
        uint32_t count;
        VkDescriptorSetLayoutBinding *binding_list;
    };

    ShaderDescriptorSet sds[size_t(DescriptorSetType::RANGE_SIZE)];

public:

    MaterialDescriptorSets(ShaderDescriptor *,const uint);
    ~MaterialDescriptorSets();

    const   ShaderDescriptorList *  GetDescriptorList   ()const {return descriptor_list;}
            ShaderDescriptorList *  GetDescriptorList   (VkDescriptorType desc_type)
    {
        if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
         ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)return nullptr;

        return descriptor_list+desc_type;
    }

    //ShaderDescriptorList &GetUBO        (){return descriptor_list[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER];}
    //ShaderDescriptorList &GetSSBO       (){return descriptor_list[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER];}
    //ShaderDescriptorList &GetUBODynamic (){return descriptor_list[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC];}
    //ShaderDescriptorList &GetSSBODynamic(){return descriptor_list[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC];}
    //ShaderDescriptorList &GetSampler    (){return descriptor_list[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER];}


    const int GetBinding(const VkDescriptorType &desc_type,const AnsiString &name)const;

    const int GetUBO(const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,name);}
    const int GetSSBO(const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,name);}
    const int GetSampler(const AnsiString &name)const{return GetBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,name);}

    const int *GetBindingList(const VkDescriptorType &desc_type)const
    {
        if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
         ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)return nullptr;

        return binding_list[(size_t)desc_type];
    }

    const int                           GetSetBindingCount(const DescriptorSetType &type)const{return sds[(size_t)type].count;}
    const VkDescriptorSetLayoutBinding *GetSetBindingList(const DescriptorSetType &type)const{return sds[(size_t)type].binding_list;}
};//class MaterialDescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_DESCRIPTOR_SETS_INCLUDE
