#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/type/Collection.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDevice.h>

namespace hgl
{
    namespace graph
    {
        GPUArrayBuffer::GPUArrayBuffer(GPUDevice *dev,VkBufferUsageFlags flags,const uint il,VkDescriptorType dt)
        {
            device=dev;
            buffer_usage_flags=flags;
            item_length=il;

            if(dt==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
             ||dt==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                offset_alignment=device->GetUBOAlign();
            else
            if(dt==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
             ||dt==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
                offset_alignment=device->GetSSBOAlign();
            else
                offset_alignment=0;

            const uint32_t unit_size=hgl_align<uint32_t>(item_length,offset_alignment);

            vk_ma=new VKMemoryAllocator(device,buffer_usage_flags,unit_size);     // construct function is going to set AllocUnitSize by minUniformOffsetAlignment
            MemoryBlock *mb=new MemoryBlock(vk_ma);

            coll=new Collection(unit_size,mb);
        }

        GPUArrayBuffer::~GPUArrayBuffer()
        {
            delete coll;
        }

        const uint32_t GPUArrayBuffer::GetUnitSize()const
        {
            return coll->GetUnitBytes();
        }

        DeviceBuffer *GPUArrayBuffer::GetBuffer()
        {
            return vk_ma->GetBuffer();
        }

        uint32 GPUArrayBuffer::Alloc(const uint32 max_count)            ///<预分配空间
        {
            if(!coll->Alloc(max_count))
                return(0);

            return coll->GetAllocCount();
        }

        void GPUArrayBuffer::Clear()
        {
            coll->Clear();
        }

        void *GPUArrayBuffer::Map(const uint32 start,const uint32 count)
        {
            return coll->Map(start,count);
        }

        void GPUArrayBuffer::Flush(const uint32 count)
        {
            vk_ma->Flush(count*GetUnitSize());
        }
    }//namespace graph
}//namespace hgl
