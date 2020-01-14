#ifndef HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE

#include<hgl/graph/shader/param/param.h>

SHADER_PARAM_NAMESPACE_BEGIN

using namespace hgl;

/**
 * 输出参数定义
 */
class OutputParam:public Param
{
public:

    using Param::Param;
    virtual ~OutputParam()=default;
};//class OutputParam:public Param

SHADER_PARAM_NAMESPACE_END
#endif//HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE
