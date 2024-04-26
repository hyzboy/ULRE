#ifndef HGL_GRAPH_RENDER_NODE_INCLUDE
#define HGL_GRAPH_RENDER_NODE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/SortedSets.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;
        class MaterialInstance;

        struct RenderNode
        {
            Matrix4f local_to_world;

            Renderable *ri;
        };

        using RenderNodeList=List<RenderNode>;

        using MaterialInstanceSets=SortedSets<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
