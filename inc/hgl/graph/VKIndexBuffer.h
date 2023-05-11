#ifndef HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>

namespace hgl
{
    namespace graph
    {
        class IndexBuffer:public DeviceBuffer
        {
            IndexType index_type;
            uint32_t count;

        private:

            friend class GPUDevice;

            IndexBuffer(VkDevice d,const DeviceBufferData &vb,IndexType it,uint32_t _count):DeviceBuffer(d,vb)
            {
                index_type=it;
                count=_count;
            }

        public:

            ~IndexBuffer()=default;

            const IndexType     GetType ()const{return index_type;}
            const uint32        GetCount()const{return count;}
        };//class IndexBuffer:public DeviceBuffer
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_INDEX_BUFFER_INCLUDE
