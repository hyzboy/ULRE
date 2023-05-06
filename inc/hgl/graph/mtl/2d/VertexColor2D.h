#ifndef HGL_GRAPH_MTL_2D_VERTEX2D_INCLUDE
#define HGL_GRAPH_MTL_2D_VERTEX2D_INCLUDE

#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl
{
    namespace graph
    {
        class MaterialCreateInfo;

        namespace mtl
        {
            struct Material2DConfig
            {
                CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

                bool                local_to_world=false;   ///<包含LocalToWorld矩阵
            };

            MaterialCreateInfo *CreateVertexColor2D(const Material2DConfig *);
        }//namespace mtl
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_MTL_2D_VERTEX2D_INCLUDE
