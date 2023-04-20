#ifndef HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE
#define HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
    {
/**
* Local 2 World 矩阵存放方案
* 
*   1.Push Constants    放弃
* 
*   2.UBO 中存放matrix4f阵列，vertex attrib中存放ID
*       UBO通常情况为16k/64k，一个matrix4f为64字节。
* 
*   3.Vertex Attribute
*/


        /**
        * 材质顶点结构
        */
        union MaterialVertexFormat
        {
            struct
            {
                uint position:3;
                bool normal:1;
                bool tangent:1;
                bool bitangent:1;
                uint color:4;
                uint texcoord:4;

                bool local2world:1;
                bool skeleton:1;
            };

            uint32 format;
        };//union MaterialVertexFormat


    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE