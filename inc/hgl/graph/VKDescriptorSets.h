#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class GPUBuffer;

class DescriptorSets
{
    VkDevice device;
    int count;
    VkDescriptorSet desc_set;
    const BindingMapping *index_by_binding;

    VkPipelineLayout pipeline_layout;

    ObjectList<VkDescriptorBufferInfo> buffer_list;
    ObjectList<VkDescriptorImageInfo> image_list;
    List<VkWriteDescriptorSet> wds_list;

private:

    friend class DescriptorSetLayoutCreater;

    DescriptorSets(VkDevice dev,const int c,VkPipelineLayout pl,VkDescriptorSet ds,const BindingMapping *bi):index_by_binding(bi)
    {
        device=dev;
        count=c;
        desc_set=ds;
        pipeline_layout=pl;
    }

public:

    ~DescriptorSets()=default;

    const uint32_t          GetCount            ()const{return count;}
    const VkDescriptorSet * GetDescriptorSets   ()const{return &desc_set;}
    const VkPipelineLayout  GetPipelineLayout   ()const{return pipeline_layout;}

    void Clear();

    bool BindUBO(const int binding,const GPUBuffer *buf,bool dynamic=false);
    bool BindUBO(const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);

    bool BindSampler(const int binding,Texture *,Sampler *);
    bool BindInputAttachment(const int binding,Texture *);
    void Update();
};//class DescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
