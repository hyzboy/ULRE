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
        class SceneNode;

        struct RenderNode
        {
            uint        index;                                          ///<在MaterialRenderList中的索引

            SceneNode * scene_node;

            uint32      l2w_version;
            uint32      l2w_index;

            Vector3f    world_position;
            float       to_camera_distance;
        };

        using RenderNodeList=List<RenderNode>;
        using RenderNodePointerList=List<RenderNode *>;

        using MaterialInstanceSets=SortedSets<MaterialInstance *>;       ///<材质实例集合
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
