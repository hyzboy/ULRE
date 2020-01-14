#ifndef HGL_GRAPH_SHADER_COMMON_INCLUDE
#define HGL_GRAPH_SHADER_COMMON_INCLUDE

#define SHADER_NAMESPACE        hgl::graph::shader
#define SHADER_NAMESPACE_BEGIN  namespace hgl{namespace graph{namespace shader{
#define SHADER_NAMESPACE_END    }}}
#define SHADER_NAMESPACE_USING  using SHADER_NAMESPACE;

#define SHADER_NODE_NAMESPACE       hgl::graph::shader::node
#define SHADER_NODE_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace shader{namespace node{
#define SHADER_NODE_NAMESPACE_END   }}}}
#define SHADER_NODE_NAMESPACE_USING using SHADER_NODE_NAMESPACE;

#define SHADER_PARAM_NAMESPACE       hgl::graph::shader::param
#define SHADER_PARAM_NAMESPACE_BEGIN namespace hgl{namespace graph{namespace shader{namespace param{
#define SHADER_PARAM_NAMESPACE_END   }}}}
#define SHADER_PARAM_NAMESPACE_USING using SHADER_PARAM_NAMESPACE;

SHADER_NODE_NAMESPACE_BEGIN
class Node;
SHADER_NODE_NAMESPACE_END

SHADER_PARAM_NAMESPACE_BEGIN
class Param;
SHADER_PARAM_NAMESPACE_END

#endif//HGL_GRAPH_SHADER_COMMON_INCLUDE
