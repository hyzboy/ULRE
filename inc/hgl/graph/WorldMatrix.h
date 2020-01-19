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
            Matrix4f ortho;                 //2D正角视图矩阵

            Matrix4f projection;
            Matrix4f inverse_projection;

            Matrix4f modelview;
            Matrix4f inverse_modelview;

            Matrix4f mvp;
            Matrix4f inverse_map;

            Vector4f view_pos;              ///<眼睛坐标
            Vector2f resolution;            ///<画布尺寸
        };//struct WorldMatrix
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_WORLD_MATRIX_INCLUDE
