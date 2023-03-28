#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKUBODynamic.h>
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

            uint32_t offset_alignment;

            Collection *coll;

        protected:

            void *          Map(const uint32 start,const uint32 count);
            void            Flush(const uint32 count);

        public:
        
            GPUArrayBuffer(GPUDevice *dev,VkBufferUsageFlags flags,const uint il,VkDescriptorType dt);
            virtual ~GPUArrayBuffer();

            const uint32_t  GetOffsetAlignment()const{return offset_alignment;}
            const uint32_t  GetUnitSize()const;
            DeviceBuffer *  GetBuffer();

            uint32          Alloc(const uint32 max_count);            ///<预分配空间
            void            Clear();

            template<typename T>
            bool            Start(UBODynamicAccess<T> *ubo_access,const uint32 start,const uint32 count)
            {
                if(!ubo_access)return(false);

                void *ptr=Map(start,count);

                if(!ptr)return(false);

                ubo_access->Start((uchar *)ptr,offset_alignment,count);
                return(true);
            }

            void            End(UBODynamicAccess<void> *ubo_access)
            {
                if(!ubo_access)return;

                Flush(ubo_access->GetCount());

                ubo_access->Restart();
            }
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
