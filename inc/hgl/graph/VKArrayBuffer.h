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
        template<typename T> class GPUArrayBuffer
        {
        protected:

            GPUDevice *device;
            VkBufferUsageFlags buffer_usage_flags;

            Collection *coll;

        public:
        
            GPUArrayBuffer(GPUDevice *dev,VkBufferUsageFlags flags)
            {
                device=dev;
                buffer_usage_flags=flags;

                {
                    uint32_t unit_size=sizeof(T);
                    VKMemoryAllocator *ma=new VKMemoryAllocator(device,buffer_usage_flags);
                    MemoryBlock *mb=new MemoryBlock(ma);

                    uint32_t align_size=ma->GetAllocUnitSize()-1;     ///< this value is "min UBO Offset alignment"

                    unit_size=(unit_size+align_size)&(~align_size);

                    coll=new Collection(unit_size,mb);
                }
            }

            virtual ~GPUArrayBuffer()
            {
                delete coll;
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

            T *Map(const uint32 start,const uint32 count)
            {
                return coll->Map(start,count);
            }
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
