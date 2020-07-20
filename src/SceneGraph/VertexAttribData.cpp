#include<hgl/graph/VertexAttribData.h>
#include<spirv_cross/spirv_common.hpp>

namespace hgl
{
    namespace graph
    {
        VertexAttribData *CreateVertexAttribData(const uint32_t base_type,const uint32_t vecsize,const uint32_t vertex_count)
        {
            VkFormat fmt;
            uint32_t stride;

            if(!vulkan::GetVulkanFormatStrideBySPIRType(fmt,stride,base_type,vecsize))
                return(nullptr);

            return(new VertexAttribData(vertex_count,vecsize,stride,fmt));
        }
    }//namespace graph
}//namespace hgl
