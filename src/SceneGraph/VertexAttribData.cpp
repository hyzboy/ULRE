#include<hgl/graph/VertexAttribData.h>
#include<hgl/graph/VKVertexInputFormat.h>

namespace hgl
{
    namespace graph
    {
        VAD *CreateVertexAttribData(const uint32_t vertex_count,const VertexInputFormat *vif)
        {
            if(vertex_count<=0
             ||vif->vec_size<1||vif->vec_size>4
             ||vif->stride<1||vif->stride>8*4
             ||!CheckVulkanFormat(vif->format))
                return(nullptr);

            return(new VertexAttribData(vertex_count,vif->format,vif->stride*vertex_count));
        }
    }//namespace graph
}//namespace hgl
