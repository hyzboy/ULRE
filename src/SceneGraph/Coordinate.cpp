#include<hgl/graph/Coordinate.h>

namespace hgl
{
    namespace graph
    {
        Matrix4f MATRIX_FROM_OPENGL_COORDINATE=scale(1,-1,1)*rotate(hgl_ang2rad(90),Vector3f(1,0,0));


    }//namespace graph
}//namespace hgl
