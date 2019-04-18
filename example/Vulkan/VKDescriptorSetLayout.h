#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SET_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SET_LAYOUT_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class DescriptorSetLayout
{
    VkDevice device;
    List<VkDescriptorSetLayout> desc_set_layout_list;

public:

    DescriptorSetLayout(VkDevice dev,const List<VkDescriptorSetLayout> &dsl_list)
    {
        device=dev;
        desc_set_layout_list=dsl_list;
    }

    ~DescriptorSetLayout();

    const uint32_t                  GetCount()const{return desc_set_layout_list.GetCount();}
    const VkDescriptorSetLayout *   GetData ()const{return desc_set_layout_list.GetData();}
};//class DescriptorSetLayout

/**
* 描述符合集创造器
*/
class DescriptorSetLayoutCreater
{
    VkDevice device;
    VkDescriptorSet desc_set;

    List<VkDescriptorSetLayoutBinding> layout_binding_list;

public:

    DescriptorSetLayoutCreater(VkDevice dev):device(dev){}
    ~DescriptorSetLayoutCreater()=default;

    void Bind(const int binding,VkDescriptorType,VkShaderStageFlagBits);

#define DESC_SET_BIND_FUNC(name,vkname) void Bind##name(const int binding,VkShaderStageFlagBits stage_flag){Bind(binding,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);}

    DESC_SET_BIND_FUNC(Sampler,     SAMPLER);
    DESC_SET_BIND_FUNC(UBO,         UNIFORM_BUFFER);
    DESC_SET_BIND_FUNC(SBO,         STORAGE_BUFFER);

    DESC_SET_BIND_FUNC(CombinedImageSampler,    COMBINED_IMAGE_SAMPLER);
    DESC_SET_BIND_FUNC(SampledImage,            SAMPLED_IMAGE);
    DESC_SET_BIND_FUNC(StorageImage,            STORAGE_IMAGE);
    DESC_SET_BIND_FUNC(UniformTexelBuffer,      UNIFORM_TEXEL_BUFFER);
    DESC_SET_BIND_FUNC(StorageTexelBuffer,      STORAGE_TEXEL_BUFFER);


    DESC_SET_BIND_FUNC(UBODynamic,  UNIFORM_BUFFER_DYNAMIC);
    DESC_SET_BIND_FUNC(SBODynamic,  STORAGE_BUFFER_DYNAMIC);

    DESC_SET_BIND_FUNC(InputAttachment,  INPUT_ATTACHMENT);

#undef DESC_SET_BIND_FUNC

    DescriptorSetLayout *Creater();
};//class DescriptorSet
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SET_LAYOUT_INCLUDE
