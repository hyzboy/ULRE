#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include"VK.h"
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class Device;

class DescriptorSetLayout
{
    Device *device;
    int count;
    VkDescriptorSetLayout *desc_set_layout_list;
    VkDescriptorSet *desc_set_list;
    Map<uint32_t,int> index_by_binding;

    VkPipelineLayout pipeline_layout;

private:

    friend class DescriptorSetLayoutCreater;

    DescriptorSetLayout(Device *dev,const int c,VkDescriptorSetLayout *dsl_list,VkPipelineLayout pl,VkDescriptorSet *desc_set,Map<uint32_t,int> &bi)
    {
        device=dev;
        count=c;
        desc_set_layout_list=dsl_list;
        desc_set_list=desc_set;
        index_by_binding=bi;
        pipeline_layout=pl;
    }

public:

    ~DescriptorSetLayout();

    const uint32_t                  GetCount            ()const{return count;}
    const VkDescriptorSetLayout *   GetLayouts          ()const{return desc_set_layout_list;}
    const VkDescriptorSet *         GetDescriptorSets   ()const{return desc_set_list;}
          VkDescriptorSet           GetDescriptorSet    (const uint32_t binding);
    const VkPipelineLayout          GetPipelineLayout   ()const{return pipeline_layout;}
};//class DescriptorSetLayout

/**
* 描述符合集创造器
*/
class DescriptorSetLayoutCreater
{
    Device *device;

    List<VkDescriptorSetLayoutBinding> layout_binding_list;

    Map<uint32_t,int> index_by_binding;

public:

    DescriptorSetLayoutCreater(Device *dev):device(dev){}
    ~DescriptorSetLayoutCreater()=default;

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

    DescriptorSetLayout *Create();
};//class DescriptorSet
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
