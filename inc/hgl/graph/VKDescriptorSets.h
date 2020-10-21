#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class Buffer;

class DescriptorSets
{
    VkDevice device;
    int count;
    VkDescriptorSet desc_set;
    const Map<uint32_t,int> *index_by_binding;

    VkPipelineLayout pipeline_layout;

    ObjectList<VkDescriptorImageInfo> desc_image_info;
    List<VkWriteDescriptorSet> wds_list;

private:

    friend class DescriptorSetLayoutCreater;

    DescriptorSets(VkDevice dev,const int c,VkPipelineLayout pl,VkDescriptorSet ds,const Map<uint32_t,int> *bi):index_by_binding(bi)
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
    bool BindUBO(const int binding,const Buffer *);
    bool BindUBODynamic(const int binding,const Buffer *);
    bool BindSampler(const int binding,Texture *,Sampler *);
    void Update();
};//class DescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
