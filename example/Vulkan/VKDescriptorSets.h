#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include"VK.h"
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class Device;

class DescriptorSetLayout
{
    Device *device;
    List<VkDescriptorSetLayout> desc_set_layout_list;
    List<VkDescriptorSet> desc_sets;

    Map<uint32_t,int> binding_index;

public:

    DescriptorSetLayout(Device *dev,const List<VkDescriptorSetLayout> &dsl_list,List<VkDescriptorSet> &ds_list,
                        Map<uint32_t,int> &bi)
    {
        device=dev;
        desc_set_layout_list=dsl_list;
        desc_sets=ds_list;
        binding_index=bi;
    }

    ~DescriptorSetLayout();

    const uint32_t                  GetCount    ()const{return desc_set_layout_list.GetCount();}
    const VkDescriptorSetLayout *   GetLayouts  ()const{return desc_set_layout_list.GetData();}

    const List<VkDescriptorSet> &   GetSets     ()const{return desc_sets;}

    bool UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info);
};//class DescriptorSetLayout

/**
* 描述符合集创造器
*/
class DescriptorSetLayoutCreater
{
    Device *device;
    VkDescriptorSet desc_set;

    List<VkDescriptorSetLayoutBinding> layout_binding_list;

    Map<uint32_t,int> binding_index;

public:

    DescriptorSetLayoutCreater(Device *dev):device(dev){}
    ~DescriptorSetLayoutCreater()=default;

    void Bind(const uint32_t binding,VkDescriptorType,VkShaderStageFlagBits);

#define DESC_SET_BIND_FUNC(name,vkname) void Bind##name(const uint32_t binding,VkShaderStageFlagBits stage_flag){Bind(binding,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);}

    DESC_SET_BIND_FUNC(Sampler,     SAMPLER);
    DESC_SET_BIND_FUNC(UBO,         UNIFORM_BUFFER);
    DESC_SET_BIND_FUNC(SSBO,        STORAGE_BUFFER);

    DESC_SET_BIND_FUNC(CombinedImageSampler,    COMBINED_IMAGE_SAMPLER);
    DESC_SET_BIND_FUNC(SampledImage,            SAMPLED_IMAGE);
    DESC_SET_BIND_FUNC(StorageImage,            STORAGE_IMAGE);
    DESC_SET_BIND_FUNC(UniformTexelBuffer,      UNIFORM_TEXEL_BUFFER);
    DESC_SET_BIND_FUNC(StorageTexelBuffer,      STORAGE_TEXEL_BUFFER);


    DESC_SET_BIND_FUNC(UBODynamic,  UNIFORM_BUFFER_DYNAMIC);
    DESC_SET_BIND_FUNC(SBODynamic,  STORAGE_BUFFER_DYNAMIC);

    DESC_SET_BIND_FUNC(InputAttachment,  INPUT_ATTACHMENT);

#undef DESC_SET_BIND_FUNC

    DescriptorSetLayout *Create();
};//class DescriptorSet
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
