#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE

#include<hgl/graph/shader/param/param.h>

BEGIN_SHADER_PARAM_NAMESPACE

#define SHADER_INPUT_PARAM(name,type) input_params.Add(new SHADER_PARAM_NAMESPACE::InputParam(#name,SHADER_PARAM_NAMESPACE::ParamType::type));

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

END_SHADER_PARAM_NAMESPACE
#endif//#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
