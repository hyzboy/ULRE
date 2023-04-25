#ifndef HGL_GRAPH_RENDER_NODE_INCLUDE
#define HGL_GRAPH_RENDER_NODE_INCLUDE

#include<hgl/math/Vector.h>
#include<hgl/type/List.h>
#include<hgl/graph/SceneInfo.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;

        struct RenderNode
        {
            Renderable *render_obj;
        };

        using RenderNodeList=List<RenderNode *>;

        struct RenderNode2D:public RenderNode
        {
            Matrix3x4f local_to_world;
        };

        using RenderNode2DList=List<RenderNode2D *>;

        constexpr double RenderNode3DDistanceFactor=100.0;

        struct RenderNode3D:public RenderNode
        {
            MVPMatrix matrix;

            Vector3f WorldCenter;

            double distance_to_camera_square;

            double distance_to_camera;

        };//struct RenderNode

        using RenderNode3DList=List<RenderNode3D *>;
    }//namespace graph
}//namespace hgl

using RenderNodePointer=hgl::graph::RenderNode *;
using RenderNodeComparator=Comparator<RenderNodePointer>;

using RenderNode2DPointer=hgl::graph::RenderNode2D *;
using RenderNode2DComparator=Comparator<RenderNode2DPointer>;

using RenderNode3DPointer=hgl::graph::RenderNode3D *;
using RenderNode3DComparator=Comparator<RenderNode3DPointer>;
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
