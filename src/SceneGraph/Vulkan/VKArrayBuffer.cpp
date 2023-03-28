#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/type/Collection.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDevice.h>

namespace hgl
{
    namespace graph
    {
        GPUArrayBuffer::GPUArrayBuffer(GPUDevice *dev,const VkBufferUsageFlags &flag,const uint il,const uint us)
        {
            device=dev;
            item_length=il;
            buffer_usage_flags=flag;

            unit_size=hgl_align<VkDeviceSize>(item_length,us);

            vk_ma=new VKMemoryAllocator(device,buffer_usage_flags,unit_size);     // construct function is going to set AllocUnitSize by minUniformOffsetAlignment
            MemoryBlock *mb=new MemoryBlock(vk_ma);

            coll=new Collection(unit_size,mb);
        }

        GPUArrayBuffer::~GPUArrayBuffer()
        {
            delete coll;
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

        GPUArrayBuffer *GPUDevice::CreateUBO(const VkDeviceSize &uint_size)
        {
            return(new GPUArrayBuffer(  this,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        uint_size,
                                        GetUBOAlign()));
        }

        GPUArrayBuffer *GPUDevice::CreateSSBO(const VkDeviceSize &uint_size)
        {
            return(new GPUArrayBuffer(  this,
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        uint_size,
                                        GetSSBOAlign()));
        }
    }//namespace graph
}//namespace hgl
