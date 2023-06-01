#ifndef HGL_GRAPH_RENDER_NODE_INCLUDE
#define HGL_GRAPH_RENDER_NODE_INCLUDE

#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;
        class Material;
        class GPUDevice;
        struct VertexInputData;
        struct IndexBufferData;

        struct RenderNode
        {
            Matrix4f local_to_world;

            Renderable *ri;
        };

        using RenderNodeList=List<RenderNode>;
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
