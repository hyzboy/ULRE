﻿#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
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

            uint unit_size;

            VKMemoryAllocator *vk_ma;

            Collection *coll;

        protected:

            void *          Map(const uint32 start,const uint32 count);
            void            Flush(const uint32 count);

        private:

            GPUArrayBuffer(VKMemoryAllocator *,const uint);

            friend class GPUDevice;

        public:
        
            virtual ~GPUArrayBuffer();

            const uint32_t  GetUnitSize()const{return unit_size;}
            DeviceBuffer *  GetBuffer();

            uint32          Alloc(const uint32 max_count);            ///<预分配空间
            void            Clear();

            template<typename T>
            bool            Start(UBODynamicAccess<T> *ubo_access,const uint32 start,const uint32 count)
            {
                if(!ubo_access)return(false);

                void *ptr=Map(start,count);

                if(!ptr)return(false);

                ubo_access->Start((uchar *)ptr,unit_size,count);
                return(true);
            }

            template<typename T>
            void            End(UBODynamicAccess<T> *ubo_access)
            {
                if(!ubo_access)return;

                Flush(ubo_access->GetCount());

                ubo_access->Restart();
            }
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
