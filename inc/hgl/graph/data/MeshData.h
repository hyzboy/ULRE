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
            int index;
            uint32 vertex_count;            ///<顶点数量

            uint8 ntb;

            float *position;
            float *normal;
            float *tangent;

            union
            {
                float *bitangent;
                float *binormal;
            };

            uint32 color_count;
            uint8 **colors;

            uint32 uv_count;
            const uint8 *uv_component;
            float **uv;

            uint32 indices_count;

            union
            {
                void *indices;
                uint16 *indices16;
                uint32 *indices32;
            };
            
            AABB bounding_box;

        public:

            MeshData()
            {
                hgl_zero(this,sizeof(MeshData));

                index=-1;
            }

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
