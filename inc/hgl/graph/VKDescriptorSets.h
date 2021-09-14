#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
VK_NAMESPACE_BEGIN
class GPUBuffer;

class DescriptorSets
{
    VkDevice device;
    int binding_count;
    VkDescriptorSet desc_set;

    VkPipelineLayout pipeline_layout;

    ObjectList<VkDescriptorBufferInfo> buffer_list;
    ObjectList<VkDescriptorImageInfo> image_list;
    List<VkWriteDescriptorSet> wds_list;

private:

    friend class GPUDevice;

    DescriptorSets(VkDevice dev,const int bc,VkPipelineLayout pl,VkDescriptorSet ds)
    {
        device          =dev;
        binding_count   =bc;
        desc_set        =ds;
        pipeline_layout =pl;
    }

public:

    ~DescriptorSets()=default;

    const uint32_t          GetCount            ()const{return binding_count;}
    const VkDescriptorSet   GetDescriptorSet    ()const{return desc_set;}
    const VkPipelineLayout  GetPipelineLayout   ()const{return pipeline_layout;}

    void Clear();

    bool BindUBO    (const int binding,const GPUBuffer *buf,bool dynamic=false);
    bool BindUBO    (const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);
    bool BindSSBO   (const int binding,const GPUBuffer *buf,bool dynamic=false);
    bool BindSSBO   (const int binding,const GPUBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);

    bool BindSampler(const int binding,Texture *,Sampler *);
    bool BindInputAttachment(const int binding,Texture *);
    void Update();
};//class DescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
