#ifndef HGL_GRAPH_RT_OUTPUT_CONFIG_INCLUDE
#define HGL_GRAPH_RT_OUTPUT_CONFIG_INCLUDE

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
    {
        struct RenderTargetOutputConfig
        {
            uint color;                 ///<要输出几个颜色缓冲区
            bool depth;                 ///<是否输出到深度缓冲区
            bool stencil;               ///<是否输出到模板缓冲区
        };
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RT_OUTPUT_CONFIG_INCLUDE
