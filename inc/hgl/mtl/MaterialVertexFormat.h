#pragma once

#include<hgl/type/DataType.h>

namespace hgl::graph
{
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
}//namespace hgl::graph
