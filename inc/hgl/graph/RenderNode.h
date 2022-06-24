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
            MVPMatrix matrix;

            Vector3f WorldCenter;

            float distance_to_camera_square;
//            float distance_to_camera;

            Renderable *ri;

        public:

            /**
             * 取得渲染对象ubo独占区大小
             */
            virtual const uint32 GetUBOBytes()const{return sizeof(MVPMatrix);}
        };//struct RenderNode

        using RenderNodeList=List<RenderNode *>;
    }//namespace graph
}//namespace hgl

using RenderNodePointer=hgl::graph::RenderNode *;
using RenderNodeComparator=Comparator<RenderNodePointer>;

#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
