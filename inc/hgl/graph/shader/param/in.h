#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE

#include<hgl/graph/shader/node/node.h>
#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/param/param.h>

BEGIN_SHADER_PARAM_NAMESPACE

#define SHADER_INPUT_PARAM(name,type) input_params.Add(new SHADER_PARAM_NAMESPACE::InputParam(#name,SHADER_PARAM_NAMESPACE::ParamType::type));

/**
 * 输入参数定义
 */
class InputParam:public Param
{
    node::Node *input_node;
    param::OutputParam *output_param;

public:

    using Param::Param;
    virtual ~InputParam()=default;

    virtual bool Join(node::Node *,OutputParam *);          ///<增加一个输入节点
    virtual void Break();                                   ///<断开一个输入节点

    virtual bool Check();                                   ///<检测当前节点是否可用
};//class InputParam:public Param

END_SHADER_PARAM_NAMESPACE
#endif//#ifndef HGL_GRAPH_SHADER_PARAM_INPUT_INCLUDE
