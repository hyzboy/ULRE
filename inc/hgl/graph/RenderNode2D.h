#ifndef HGL_GRAPH_RENDER_NODE_2D_INCLUDE
#define HGL_GRAPH_RENDER_NODE_2D_INCLUDE

#include<hgl/math/Math.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;

        struct RenderNode2D
        {
            Matrix3x4f local_to_world;

            Renderable *ri;
        };

        using RenderNode2DList=List<RenderNode2D *>;
    }//namespace graph
}//namespace hgl

using RenderNode2DPointer=hgl::graph::RenderNode2D *;
using RenderNode2DComparator=Comparator<RenderNode2DPointer>;
#endif//HGL_GRAPH_RENDER_NODE_2D_INCLUDE
