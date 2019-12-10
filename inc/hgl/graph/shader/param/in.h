#ifndef HGL_GRAPH_SHADER_INPUT_PARAM_INCLUDE
#define HGL_GRAPH_SHADER_INPUT_PARAM_INCLUDE

#include<hgl/graph/shader/param/param.h>

BEGIN_SHADER_NODE_NAMESPACE

#define SHADER_INPUT_PARAM(name,type) input_params.Add(new InputParam(#name,ParamType::type));

/**
 * 输入参数定义
 */
class InputParam:public Param
{
public:

    using Param::Param;
    virtual ~InputParam()=default;
};//class InputParam:public Param

/**
 * 数值类输入参数定义
 */
template<typename T>
class InputParamNumber:public InputParam
{
    bool is_const;
    T min_value;
    T max_value;

public:
};//class InputParamNumber:public InputParam

END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_INPUT_PARAM_INCLUDE
