#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class Device;

class DescriptorSets
{
    Device *device;
    int count;
    VkDescriptorSet desc_set;
    const Map<uint32_t,int> *index_by_binding;

    VkPipelineLayout pipeline_layout;

    List<VkDescriptorImageInfo> desc_image_info;
    List<VkWriteDescriptorSet> write_desc_sets;

private:

    friend class DescriptorSetLayoutCreater;

    DescriptorSets(Device *dev,const int c,VkPipelineLayout pl,VkDescriptorSet ds,const Map<uint32_t,int> *bi):index_by_binding(bi)
    {
        device=dev;
        count=c;
        desc_set=ds;
        pipeline_layout=pl;
    }

public:

    ~DescriptorSets()=default;

    const uint32_t                  GetCount            ()const{return count;}
    const VkDescriptorSet *         GetDescriptorSets   ()const{return &desc_set;}
    const VkPipelineLayout          GetPipelineLayout   ()const{return pipeline_layout;}

    void Clear();
    bool BindUBO(const uint32_t binding,const VkDescriptorBufferInfo *);
    bool BindUBODynamic(const uint32_t binding,const VkDescriptorBufferInfo *);
    bool BindSampler(const uint32_t binding,Texture *,Sampler *);
    void Update();
};//class DescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
