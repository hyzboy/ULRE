#include<hgl/graph/Coordinate.h>

namespace hgl
{
    namespace graph
    {
        const Matrix4f GetOpenGL2VulkanMatrix()
        {
            const Matrix4f MATRIX_FROM_OPENGL_COORDINATE=scale(1,-1,1)*rotate(HGL_RAD_90,Vector3f(1,0,0));

            return MATRIX_FROM_OPENGL_COORDINATE;
        }
    }//namespace graph
}//namespace hgl
