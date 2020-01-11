#ifndef HGL_GRAPH_WORLD_MATRIX_INCLUDE
#define HGL_GRAPH_WORLD_MATRIX_INCLUDE

#include<hgl/math/Math.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 世界矩阵数据
         * @see res/shader/UBO_WorldMatrix.glsl
         */
        struct WorldMatrix
        {
            alignas(16) Matrix4f ortho;                 //2D正角视图矩阵

            alignas(16) Matrix4f projection;
            alignas(16) Matrix4f inverse_projection;

            alignas(16) Matrix4f modelview;
            alignas(16) Matrix4f inverse_modelview;

            alignas(16) Matrix4f mvp;
            alignas(16) Matrix4f inverse_map;

            alignas(16) Vector4f view_pos;              ///<眼睛坐标
        };//struct WorldMatrix
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_WORLD_MATRIX_INCLUDE
