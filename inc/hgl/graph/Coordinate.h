#ifndef HGL_GRAPH_COORDINATE_INCLUDE
#define HGL_GRAPH_COORDINATE_INCLUDE

#include<hgl/math/Math.h>

namespace hgl
{
    namespace graph
    {
        /*
         * OpenGL Coordinate System         Vulkan Coordinate System        My Coordinate System
         *
         *                                         /Z
         *     Y|    /Z                           /                             Z|    /Y
         *      |   /                            /                               |   /        
         *      |  /                            *------------                    |  /         
         *      | /                             |           X                    | /          
         *      |/                              |                                |/           
         *      *------------                   |                                *------------
         *                  X                   | Y                                          X
         */

        extern Matrix4f MATRIX_FROM_OPENGL_COORDINATE;              //OpenGL坐标系数据到我方坐标系数据变换用矩阵

        }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_COORDINATE_INCLUDE
