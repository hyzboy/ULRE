#ifndef HGL_GRAPH_MESH_DATA_INCLUDE
#define HGL_GRAPH_MESH_DATA_INCLUDE

#include<hgl/type/DataType.h>
#include<hgl/math/Math.h>

namespace hgl
{
    namespace graph
    {        
        struct MeshData
        {
            int index=-1;
            uint32 vertex_count=0;            ///<顶点数量

            uint8 ntb=0;

            float *position;
            float *normal;
            float *tangent;

            union
            {
                float *bitangent;
                float *binormal;
            };

            uint32 color_count=0;
            uint8 **colors=nullptr;

            uint32 uv_count=0;
            const uint8 *uv_component=nullptr;
            float **uv=nullptr;

            uint32 indices_count=0;

            union
            {
                void *indices;
                uint16 *indices16;
                uint32 *indices32;
            };
            
            AABB bounding_box;

        public:

            ~MeshData()
            {
                if(colors)
                    delete[] colors;

                if(uv)
                    delete[] uv;
            }
        };//struct MeshData
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MESH_DATA_INCLUDE
