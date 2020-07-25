#include<hgl/graph/VertexAttribData.h>

namespace hgl
{
    namespace graph
    {
        VertexAttribData *CreateVertexAttribData(const vulkan::BaseType base_type,const uint32_t vecsize,const uint32_t vertex_count)
        {
            VkFormat fmt;
            uint32_t stride;

            if(!vulkan::GetVulkanFormatStride(fmt,stride,base_type,vecsize))
                return(nullptr);

            return(new VertexAttribData(vertex_count,vecsize,stride,fmt));
        }
    }//namespace graph
}//namespace hgl
