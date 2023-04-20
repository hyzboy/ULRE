#ifndef HGL_GRAPH_MTL_SHADER_BUFFER_INCLUDE
#define HGL_GRAPH_MTL_SHADER_BUFFER_INCLUDE

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
    {
        struct ShaderBufferSource
        {
            const char *struct_name;
            const char *value_name;
            const char *codes;
        };
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MTL_SHADER_BUFFER_INCLUDE
