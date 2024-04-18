#ifndef HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>

namespace hgl
{
    namespace graph
    {
        class IndexBuffer:public DeviceBuffer
        {
            IndexType   index_type;
            uint        stride;
            uint32_t    count;

        private:

            friend class GPUDevice;

            IndexBuffer(VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count):DeviceBuffer(d,vb)
            {
                index_type=it;
                count=_count;

                if(index_type==IndexType::U16)stride=2;else
                if(index_type==IndexType::U32)stride=4;else
                if(index_type==IndexType::U8)stride=1;else
                    stride=0;
            }

        public:

            ~IndexBuffer()=default;

            const IndexType     GetType     ()const{return index_type;}
            const uint          GetStride   ()const{return stride;}
            const uint32        GetCount    ()const{return count;}
        };//class IndexBuffer:public DeviceBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE
