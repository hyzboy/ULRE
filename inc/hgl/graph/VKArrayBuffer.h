#ifndef HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKDynamicBufferAccess.h>
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

            uint align_size;
            uint range_size;

            VKMemoryAllocator *vk_ma;

            Collection *coll;

        protected:

            void *          Map(const uint32 start,const uint32 count);
            void            Flush(const uint32 count);

        private:

            GPUArrayBuffer(VKMemoryAllocator *,const uint,const uint);

            friend class GPUDevice;

        public:
        
            virtual ~GPUArrayBuffer();

            const uint32_t  GetAlignSize()const{return align_size;}     ///<数据对齐字节数
            const uint32_t  GetRangeSize()const{return range_size;}     ///<单次渲染访问最大字节数

            DeviceBuffer *  GetBuffer();

            uint32          Alloc(const uint32 max_count);              ///<预分配空间
            void            Clear();

            template<typename T>
            bool            Start(DynamicBufferAccess<T> *dba,const uint32 start,const uint32 count)
            {
                if(!dba)return(false);

                void *ptr=Map(start,count);

                if(!ptr)return(false);

                dba->Start((uchar *)ptr,align_size,count);
                return(true);
            }

            template<typename T>
            void            End(DynamicBufferAccess<T> *dba)
            {
                if(!dba)return;

                Flush(dba->GetCount());

                dba->Restart();
            }
        };//class GPUArrayBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_ARRAY_BUFFER_INCLUDE
