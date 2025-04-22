#ifndef HGL_GRAPH_VULKAN_DESCRIPTOR_SET_INCLUDE
#define HGL_GRAPH_VULKAN_DESCRIPTOR_SET_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/ObjectList.h>
#include<hgl/type/SortedSet.h>
VK_NAMESPACE_BEGIN
class DeviceBuffer;

class DescriptorSet
{
    VkDevice device;
    int vab_count;
    VkDescriptorSet desc_set;

    VkPipelineLayout pipeline_layout;

    ObjectList<VkDescriptorBufferInfo> vab_list;
    ObjectList<VkDescriptorImageInfo> image_list;
    ArrayList<VkWriteDescriptorSet> wds_list;

    SortedSet<uint32_t> binded_sets;

    bool is_dirty;

public:

    DescriptorSet(VkDevice dev,const int bc,VkPipelineLayout pl,VkDescriptorSet ds)
    {
        device          =dev;
        vab_count   =bc;
        desc_set        =ds;
        pipeline_layout =pl;

        is_dirty=true;
    }

    ~DescriptorSet()=default;

    const uint32_t          GetCount            ()const{return vab_count;}
    const VkDescriptorSet   GetDescriptorSet    ()const{return desc_set;}
    const VkPipelineLayout  GetPipelineLayout   ()const{return pipeline_layout;}

    const bool              IsReady             ()const{return wds_list.GetCount()==vab_count;}

    void Clear();

    bool BindUBO    (const int binding,const DeviceBuffer *buf,bool dynamic=false);
    bool BindUBO    (const int binding,const DeviceBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);
    bool BindSSBO   (const int binding,const DeviceBuffer *buf,bool dynamic=false);
    bool BindSSBO   (const int binding,const DeviceBuffer *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);

    bool BindImageSampler(const int binding,Texture *,Sampler *);
    bool BindInputAttachment(const int binding,ImageView *);
    void Update();
};//class DescriptorSet
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SET_INCLUDE
