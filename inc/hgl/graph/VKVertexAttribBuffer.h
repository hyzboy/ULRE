#ifndef HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>

namespace hgl
{
    namespace graph
    {
        class VertexAttribBuffer:public GPUBuffer
        {
            VkFormat format;                    ///<���ݸ�ʽ
            uint32_t stride;                    ///<���������ֽ���
            uint32_t count;                     ///<��������

        private:

            friend class GPUDevice;

            VertexAttribBuffer(VkDevice d,const GPUBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):GPUBuffer(d,vb)
            {
                format=fmt;
                stride=_stride;
                count=_count;
            }

        public:

            ~VertexAttribBuffer()=default;

            const VkFormat GetFormat()const { return format; }
            const uint32_t GetStride()const { return stride; }
            const uint32_t GetCount ()const { return count; }

            void *Map(VkDeviceSize start=0,VkDeviceSize size=0) override
            {
                return GPUBuffer::Map(start*stride,size*stride);
            }
        };//class VertexAttribBuffer:public GPUBuffer

        using VBO=VertexAttribBuffer;
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE
