#ifndef HGL_GRAPH_SHADER_OUTPUT_PARAM_INCLUDE
#define HGL_GRAPH_SHADER_OUTPUT_PARAM_INCLUDE

#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/node/node.h>
#include<hgl/type/Set.h>
#include<hgl/type/Pair.h>

BEGIN_SHADER_NODE_NAMESPACE

#define SHADER_OUTPUT_PARAM(name,type) output_params.Add(new Param(#name,ParamType::type));

using namespace hgl;

using InputNode=Pair<node::Node *,InputParam *>;

/**
 * 输出参数定义
 */
class OutputParam:public Param
{
    Set<InputNode> join_param;                              //对应的多个输入节点

public:

    using Param::Param;
    virtual ~OutputParam()=default;

    bool Join(node::Node *,InputParam *ip);                 //增加一个输入节点
    void Break(node::Node *,InputParam *ip);                //断开一个输入节点
    void BreakAll();                                        //断开所有输入节点
};//class OutputParam:public Param

END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_OUTPUT_PARAM_INCLUDE
