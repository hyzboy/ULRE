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
            MaterialCreateInfo *CreateVertexColor2D(const CoordinateSystem2D &);
        }//namespace mtl
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_MTL_2D_VERTEX2D_INCLUDE
