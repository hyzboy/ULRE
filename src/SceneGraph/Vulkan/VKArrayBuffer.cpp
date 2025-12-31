#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/type/Collection.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDevice.h>

namespace hgl
{
    namespace graph
    {
        VulkanArrayBuffer *VulkanDevice::CreateArrayInUBO(const VkDeviceSize &item_length)
        {
            const uint align_size=hgl_align<VkDeviceSize>(item_length,GetUBOAlign());
            
            auto vk_ma=new VKMemoryAllocator(this,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,align_size);

            return(new VulkanArrayBuffer(vk_ma,align_size,GetUBORange()));
        }

        VulkanArrayBuffer *VulkanDevice::CreateArrayInSSBO(const VkDeviceSize &item_length)
        {
            const uint align_size=hgl_align<VkDeviceSize>(item_length,GetSSBOAlign());
            
            auto vk_ma=new VKMemoryAllocator(this,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,align_size);

            return(new VulkanArrayBuffer(vk_ma,align_size,GetSSBORange()));
        }
    }//namespace graph

    namespace graph
    {
        VulkanArrayBuffer::VulkanArrayBuffer(VKMemoryAllocator *va,const uint as,const uint rs)
        {
            vk_ma=va;
            align_size=as;
            range_size=rs;

            MemoryBlock *mb=new MemoryBlock(vk_ma);

            coll=new Collection(align_size,mb);
        }

        VulkanArrayBuffer::~VulkanArrayBuffer()
        {
            delete coll;
        }

        DeviceBuffer *VulkanArrayBuffer::GetBuffer()
        {
            return vk_ma->GetBuffer();
        }

        uint32 VulkanArrayBuffer::Reserve(const uint32 max_count)            ///<预分配空间
        {
            if(!coll->Reserve(max_count))
                return(0);

            return coll->GetAllocCount();
        }

        void VulkanArrayBuffer::Clear()
        {
            coll->Clear();
        }

        void *VulkanArrayBuffer::Map(const uint32 start,const uint32 count)
        {
            return coll->Map(start,count);
        }

        void VulkanArrayBuffer::Flush(const uint32 count)
        {
            vk_ma->Flush(count*align_size);
        }
    }//namespace graph
}//namespace hgl
