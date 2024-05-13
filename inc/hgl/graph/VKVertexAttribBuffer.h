#ifndef HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE

#include<hgl/graph/VKBuffer.h>

namespace hgl
{
    namespace graph
    {
        class VertexAttribBuffer:public DeviceBuffer
        {
            VkFormat format;                    ///<数据格式
            uint32_t stride;                    ///<单个数据字节数
            uint32_t count;                     ///<数据数量

        private:

            friend class GPUDevice;

            VertexAttribBuffer(VkDevice d,const DeviceBufferData &vb,VkFormat fmt,uint32_t _stride,uint32_t _count):DeviceBuffer(d,vb)
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

            const VkDeviceSize GetBytes()const { return stride*count; }
        };//class VertexAttribBuffer:public DeviceBuffer

        using VAB=VertexAttribBuffer;

        class VABView
        {
        public:

            VABView()=default;
            virtual ~VABView()=default;

            virtual VAB *GetVAB()=0;
            virtual VkDeviceSize GetStart()const=0;
            virtual VkDeviceSize GetSize()const=0;

            virtual void *Map()=0;
            virtual void Unmap()=0;
        };//class VABView
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VULKAN_VERTEX_ATTRIB_BUFFER_INCLUDE
