#ifndef HGL_GRAPH_SHADER_COMMON_INCLUDE
#define HGL_GRAPH_SHADER_COMMON_INCLUDE

#define SHADER_NAMESPACE        hgl::graph::shader
#define BEGIN_SHADER_NAMESPACE  namespace hgl{namespace graph{namespace shader{
#define END_SHADER_NAMESPACE    }}}

#define SHADER_NODE_NAMESPACE       hgl::graph::shader::node
#define BEGIN_SHADER_NODE_NAMESPACE namespace hgl{namespace graph{namespace shader{namespace node{
#define END_SHADER_NODE_NAMESPACE   }}}}

#define SHADER_PARAM_NAMESPACE       hgl::graph::shader::param
#define BEGIN_SHADER_PARAM_NAMESPACE namespace hgl{namespace graph{namespace shader{namespace param{
#define END_SHADER_PARAM_NAMESPACE   }}}}

BEGIN_SHADER_NODE_NAMESPACE
class Node;
END_SHADER_NODE_NAMESPACE

BEGIN_SHADER_PARAM_NAMESPACE
class Param;
class InputParam:public Param;
class OutputParam:public Param;
END_SHADER_PARAM_NAMESPACE
#endif//HGL_GRAPH_SHADER_COMMON_INCLUDE
