#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
namespace hgl
{
    class Collection;

    namespace graph
    {
        class VKMemoryAllocator;

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
        
            GPUArrayBuffer(GPUDevice *dev,VkBufferUsageFlags flags,const uint il);
            virtual ~GPUArrayBuffer();

            const uint32_t  GetOffsetAlignment()const{return ubo_offset_alignment;}
            const uint32_t  GetUnitSize()const;
            DeviceBuffer *  GetBuffer();

            uint32          Alloc(const uint32 max_count);            ///<预分配空间
            void            Clear();

            void *          Map(const uint32 start,const uint32 count);
            void            Flush(const uint32 count);
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
