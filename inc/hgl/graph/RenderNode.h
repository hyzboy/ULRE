#ifndef HGL_GRAPH_RENDER_NODE_INCLUDE
#define HGL_GRAPH_RENDER_NODE_INCLUDE

#include<hgl/math/Vector.h>
#include<hgl/type/List.h>
namespace hgl
{
    namespace graph
    {
        class RenderableInstance;

        struct RenderNode
        {
            Vector4f WorldCenter;

            float distance_to_camera_square;
//            float distance_to_camera;

            RenderableInstance *ri;
        };//struct RenderNode

        using RenderNodeList=List<RenderNode *>;
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
