#include<hgl/graph/VertexAttribData.h>

namespace hgl
{
    namespace graph
    {
        VAD *CreateVertexAttribData(const VertexAttribType *type,const uint32_t vertex_count)
        {
            if(!type||!type->Check())
                return(nullptr);

            VkFormat fmt=GetVulkanFormat(type);

            return(new VertexAttribData(vertex_count,type->vec_size,type->GetStride(),fmt));
        }
    }//namespace graph
}//namespace hgl
