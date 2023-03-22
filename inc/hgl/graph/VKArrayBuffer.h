#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMemoryAllocator.h>
#include<hgl/type/Collection.h>
namespace hgl
{
    namespace graph
    {
        /**
        * GPU数据阵列缓冲区<br>
        * 它用于储存多份相同格式的数据，常用于多物件渲染，instance等
        */
        class GPUArrayBuffer
        {
        protected:

            GPUDevice *device;
            VkBufferUsageFlags buffer_usage_flags;

            uint item_length;                        ///<单个数据长度

            VKMemoryAllocator *vk_ma;

            uint32_t ubo_offset_alignment;

            Collection *coll;

        public:
        
            GPUArrayBuffer(GPUDevice *dev,VkBufferUsageFlags flags,const uint il)
            {
                device=dev;
                buffer_usage_flags=flags;
                item_length=il;

                {
                    ubo_offset_alignment=device->GetUBOAlign();

                    const uint32_t unit_size=hgl_align<uint32_t>(item_length,ubo_offset_alignment);

                    vk_ma=new VKMemoryAllocator(device,buffer_usage_flags,unit_size);     // construct function is going to set AllocUnitSize by minUniformOffsetAlignment
                    MemoryBlock *mb=new MemoryBlock(vk_ma);

                    coll=new Collection(unit_size,mb);
                }
            }

            virtual ~GPUArrayBuffer()
            {
                delete coll;
            }

            const uint32_t GetOffsetAlignment()const
            {
                return ubo_offset_alignment;
            }

            const uint32_t GetUnitSize()const
            {
                return coll->GetUnitBytes();
            }

            DeviceBuffer *GetBuffer()
            {
                return vk_ma->GetBuffer();
            }

            uint32 Alloc(const uint32 max_count)            ///<预分配空间
            {
                if(!coll->Alloc(max_count))
                    return(0);

                return coll->GetAllocCount();
            }

            void Clear()
            {
                coll->Clear();
            }

            void *Map(const uint32 start,const uint32 count)
            {
                return coll->Map(start,count);
            }

            void Flush(const uint32 count)
            {
                vk_ma->Flush(count*GetUnitSize());
            }
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
