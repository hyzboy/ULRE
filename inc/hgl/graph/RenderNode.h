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

        constexpr double RenderNode3DDistanceFactor=100.0;

        struct RenderNode3D
        {
            MVPMatrix matrix;

            Vector3f WorldCenter;

            double distance_to_camera_square;

            double distance_to_camera;

            Renderable *ri;

        public:

            /**
             * 取得渲染对象ubo独占区大小
             */
            virtual const uint32 GetUBOBytes()const{return sizeof(MVPMatrix);}
        };//struct RenderNode

        using RenderNode3DList=List<RenderNode3D *>;
    }//namespace graph
}//namespace hgl

using RenderNode3DPointer=hgl::graph::RenderNode3D *;
using RenderNode3DComparator=Comparator<RenderNode3DPointer>;
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
