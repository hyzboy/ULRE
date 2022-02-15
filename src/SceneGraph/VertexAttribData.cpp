#include<hgl/graph/VertexAttribData.h>

namespace hgl
{
    namespace graph
    {
        VAD *CreateVertexAttribData(const uint32_t vertex_count,const VkFormat fmt,const int vec_size,const uint stride)
        {
            if(vertex_count<=0
             ||vec_size<1||vec_size>4
             ||stride<1||stride>8*4
             ||!CheckVulkanFormat(fmt))
                return(nullptr);

            return(new VertexAttribData(vertex_count,vec_size,stride,fmt));
        }
    }//namespace graph
}//namespace hgl
