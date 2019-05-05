#pragma once

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class Device;
class DescriptorSets;

/**
* 描述符合集创造器
*/
class DescriptorSetLayoutCreater
{
    Device *device;

    List<VkDescriptorSetLayoutBinding> layout_binding_list;
    VkDescriptorSetLayout *dsl_list=nullptr;
    Map<uint32_t,int> index_by_binding;

    VkPipelineLayout pipeline_layout=nullptr;

public:

    DescriptorSetLayoutCreater(Device *dev):device(dev){}
    ~DescriptorSetLayoutCreater();

    void Bind(const uint32_t binding,VkDescriptorType,VkShaderStageFlagBits);
    void Bind(const uint32_t *binding,const uint32_t count,VkDescriptorType type,VkShaderStageFlagBits stage);

#define DESC_SET_BIND_FUNC(name,vkname) void Bind##name(const uint32_t binding,VkShaderStageFlagBits stage_flag){Bind(binding,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);} \
                                        void Bind##name(const uint32_t *binding,const uint32_t count,VkShaderStageFlagBits stage_flag){Bind(binding,count,VK_DESCRIPTOR_TYPE_##vkname,stage_flag);}

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

    bool CreatePipelineLayout();

    const VkPipelineLayout GetPipelineLayout()const{return pipeline_layout;}

    DescriptorSets *Create();
};//class DescriptorSetLayoutCreater
VK_NAMESPACE_END