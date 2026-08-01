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
class DeviceBuffer;
class IGPUBuffer;

class DescriptorSet
{
    OBJECT_LOGGER

    VkDevice device;
    int binding_capacity;
    VkDescriptorSet desc_set;

    VkPipelineLayout pipeline_layout;

    ValueArray<VkDescriptorBufferInfo> buffer_info_list;
    ValueArray<VkDescriptorImageInfo> image_info_list;
    ValueArray<VkWriteDescriptorSet> write_descriptor_sets;
    ValueArray<int> write_buffer_info_indices;
    ValueArray<int> write_image_info_indices;

    OrderedSet<uint32_t> touched_bindings;

    bool is_dirty;

private:

    int FindWriteDescriptorIndex(const int binding,const VkDescriptorType desc_type) const;
    void SyncWriteDescriptorInfoPointers();
    bool UpdateOrAppendBufferBinding(const int binding,const VkDescriptorType desc_type,const VkDescriptorBufferInfo &new_info);
    bool UpdateOrAppendImageBinding(const int binding,const VkDescriptorType desc_type,const VkDescriptorImageInfo &new_info);

public:

    DescriptorSet(VkDevice dev,const int bc,VkPipelineLayout pl,VkDescriptorSet ds)
    {
        device          =dev;
        binding_capacity=bc;
        desc_set        =ds;
        pipeline_layout =pl;

        buffer_info_list.Reserve(binding_capacity);
        image_info_list.Reserve(binding_capacity);
        write_descriptor_sets.Reserve(binding_capacity);
        write_buffer_info_indices.Reserve(binding_capacity);
        write_image_info_indices.Reserve(binding_capacity);

        is_dirty=true;
    }

    ~DescriptorSet()=default;

    const uint32_t          GetCount            ()const{return binding_capacity;}
    const VkDescriptorSet   GetDescriptorSet    ()const{return desc_set;}
    const VkPipelineLayout  GetPipelineLayout   ()const{return pipeline_layout;}

    const bool              IsReady             ()const{return write_descriptor_sets.GetCount()==binding_capacity;}

    // Debug/Test accessors: used by descriptor lifetime regression tests
    const ValueArray<VkDescriptorBufferInfo> &DebugGetBufferInfoList() const { return buffer_info_list; }
    const ValueArray<VkDescriptorImageInfo>  &DebugGetImageInfoList () const { return image_info_list; }
    const ValueArray<VkWriteDescriptorSet>   &DebugGetWriteSetList  () const { return write_descriptor_sets; }

    void Clear();

    bool BindUBO    (const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);
    bool BindSSBO   (const int binding,const VkBufferOwner *buf,const VkDeviceSize offset,const VkDeviceSize range,bool dynamic=false);

    bool BindUBO    (const int binding,const IGPUBuffer *gpu,bool dynamic=false);
    bool BindSSBO   (const int binding,const IGPUBuffer *gpu,bool dynamic=false);

    bool BindTexture(const int binding,Texture *);
    bool BindTextureSampler(const int binding,Texture *,Sampler *);
    bool BindInputAttachment(const int binding,ImageView *);
    void Update();
};//class DescriptorSet
}//namespace hgl::graph
