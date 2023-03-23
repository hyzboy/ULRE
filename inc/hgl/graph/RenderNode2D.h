#ifndef HGL_GRAPH_RENDER_NODE_2D_INCLUDE
#define HGL_GRAPH_RENDER_NODE_2D_INCLUDE

#include<hgl/math/Vector.h>
#include<hgl/type/List.h>
#include<hgl/graph/SceneInfo.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;

        struct Transiton2D
        {
            Vector2f move;
            Vector2f center;        //中心点

            //下方的不管是缩放还是旋转，均以上面的center为中心变换

            Vector2f scale;
            float rotate;
            float z;
        };

        struct RenderNode2D
        {
            Transiton2D trans;            

            Renderable *ri;
        };

        using RenderNode2DList=List<RenderNode2D *>;
    }//namespace graph
}//namespace hgl

using RenderNode2DPointer=hgl::graph::RenderNode2D *;
using RenderNode2DComparator=Comparator<RenderNode2DPointer>;
#endif//HGL_GRAPH_RENDER_NODE_2D_INCLUDE
