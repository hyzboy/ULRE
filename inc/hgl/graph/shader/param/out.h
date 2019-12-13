#ifndef HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE

#include<hgl/graph/shader/param/param.h>

BEGIN_SHADER_PARAM_NAMESPACE

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

END_SHADER_PARAM_NAMESPACE
#endif//HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE
