#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/type/ManagedArray.h>
#include<hgl/log/Log.h>

namespace hgl::graph{ class VkBufferOwner; }  // forward-decl for BindUBO/BindSSBO offset+range params

// Comparison operators for Vulkan structures used in ValueArray
inline bool operator==(const VkDescriptorBufferInfo& lhs, const VkDescriptorBufferInfo& rhs) {
    return lhs.buffer == rhs.buffer &&
           lhs.offset == rhs.offset &&
           lhs.range == rhs.range;
}

inline bool operator==(const VkDescriptorImageInfo& lhs, const VkDescriptorImageInfo& rhs) {
    return lhs.sampler == rhs.sampler &&
           lhs.imageView == rhs.imageView &&
           lhs.imageLayout == rhs.imageLayout;
}

inline bool operator==(const VkWriteDescriptorSet& lhs, const VkWriteDescriptorSet& rhs) {
    return lhs.sType == rhs.sType &&
           lhs.pNext == rhs.pNext &&
           lhs.dstSet == rhs.dstSet &&
           lhs.dstBinding == rhs.dstBinding &&
           lhs.dstArrayElement == rhs.dstArrayElement &&
           lhs.descriptorCount == rhs.descriptorCount &&
           lhs.descriptorType == rhs.descriptorType &&
           lhs.pImageInfo == rhs.pImageInfo &&
           lhs.pBufferInfo == rhs.pBufferInfo &&
           lhs.pTexelBufferView == rhs.pTexelBufferView;
}

namespace hgl::graph{
class VKDescriptorBuffer;
class IGPUBuffer;

class DescriptorSet
{
    OBJECT_LOGGER

    VkDevice device;
    int vab_count;
    VkDescriptorSet desc_set;

    VkPipelineLayout pipeline_layout;

    std::vector<VkDescriptorBufferInfo> vab_list;
    std::vector<VkDescriptorImageInfo> image_list;
    std::vector<VkWriteDescriptorSet> wds_list;
    std::vector<int> wds_buffer_info_indices;
    std::vector<int> wds_image_info_indices;

    OrderedSet<uint32_t> binded_sets;

    bool is_dirty;

private:

    void SyncWriteDescriptorInfoPointers();

public:

    DescriptorSet(VkDevice dev,const int bc,VkPipelineLayout pl,VkDescriptorSet ds)
    {
        device          =dev;
        vab_count   =bc;
        desc_set        =ds;
        pipeline_layout =pl;

        vab_list.reserve(vab_count);
        image_list.reserve(vab_count);
        wds_list.reserve(vab_count);
        wds_buffer_info_indices.reserve(vab_count);
        wds_image_info_indices.reserve(vab_count);

        is_dirty=true;
    }

    ~DescriptorSet()=default;

    const uint32_t          GetCount            ()const{return vab_count;}
    const VkDescriptorSet   GetDescriptorSet    ()const{return desc_set;}
    const VkPipelineLayout  GetPipelineLayout   ()const{return pipeline_layout;}

    const bool              IsReady             ()const{return (int)wds_list.size()==vab_count;}

    // Debug/Test accessors: used by descriptor lifetime regression tests
    const std::vector<VkDescriptorBufferInfo> &DebugGetBufferInfoList() const { return vab_list; }
    const std::vector<VkDescriptorImageInfo>  &DebugGetImageInfoList () const { return image_list; }
    const std::vector<VkWriteDescriptorSet>   &DebugGetWriteSetList  () const { return wds_list; }

    void Clear();

    bool BindUBO    (const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);
    bool BindSSBO   (const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);

    bool BindUBO    (const int binding,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO   (const int binding,const IGPUBuffer *gpu,bool dynamic=false);

    bool BindTexture(const int binding,Texture *);
    bool BindResourceSampler(const int binding,Texture *,Sampler *);
    bool BindInputAttachment(const int binding,ImageView *);
    void Update();
};//class DescriptorSet
}//namespace hgl::graph
