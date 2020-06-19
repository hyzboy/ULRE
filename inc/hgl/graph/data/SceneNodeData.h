#ifndef HGL_GRAPH_SCENE_NODE_DATA_INCLUDE
#define HGL_GRAPH_SCENE_NODE_DATA_INCLUDE

#include<hgl/graph/data/MeshData.h>
#include<hgl/type/StringList.h>

namespace hgl
{
    namespace graph
    {        
        struct SceneNodeData
        {
            UTF8String name;

            Matrix4f local_matrix;

            List<uint32> mesh_index;

            ObjectList<SceneNodeData> sub_nodes;
        };//struct SceneNodeData

        struct ModelData
        {
            uint8 *source_filedata=nullptr;

            uint8 version=0;

            UTF8StringList mesh_name;
            ObjectList<MeshData> mesh_list;
            
            SceneNodeData *root_node=nullptr;

            AABB bounding_box;

        public:

            ~ModelData()
            {
                SAFE_CLEAR(root_node);
                delete[] source_filedata;
            }
        };//struct ModelData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_NODE_DATA_INCLUDE
