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
    VkDescriptorSet *desc_set_list;
    const Map<uint32_t,int> *index_by_binding;

    VkPipelineLayout pipeline_layout;

private:

    friend class DescriptorSetLayoutCreater;

    DescriptorSets(Device *dev,const int c,VkPipelineLayout pl,VkDescriptorSet *desc_set,const Map<uint32_t,int> *bi):index_by_binding(bi)
    {
        device=dev;
        count=c;
        desc_set_list=desc_set;
        pipeline_layout=pl;
    }

public:

    ~DescriptorSets();

    const uint32_t                  GetCount            ()const{return count;}
    const VkDescriptorSet *         GetDescriptorSets   ()const{return desc_set_list;}
          VkDescriptorSet           GetDescriptorSet    (const uint32_t binding)const;
    const VkPipelineLayout          GetPipelineLayout   ()const{return pipeline_layout;}

    bool UpdateUBO(const uint32_t binding,const VkDescriptorBufferInfo *buf_info);
    //bool UpdateUBO(const UTF8String &name,const VkDescriptorBufferInfo *buf_info)
    //{
    //    if(name.IsEmpty()||!buf_info)
    //        return(false);

    //    return UpdateUBO(GetUBOBinding(name),buf_info);
    //}
};//class DescriptorSets
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DESCRIPTOR_SETS_LAYOUT_INCLUDE
