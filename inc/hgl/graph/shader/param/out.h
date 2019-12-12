#ifndef HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_OUTPUT_INCLUDE

#include<hgl/graph/shader/param/param.h>
#include<hgl/type/Set.h>
#include<hgl/type/Pair.h>

BEGIN_SHADER_PARAM_NAMESPACE

using namespace hgl;

#define SHADER_OUTPUT_PARAM(name,type) output_params.Add(new SHADER_PARAM_NAMESPACE::Param(#name,SHADER_PARAM_NAMESPACE::ParamType::type));

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
