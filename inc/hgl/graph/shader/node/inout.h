#ifndef HGL_GRAPH_SHADER_NODE_INOUT_INCLUDE
#define HGL_GRAPH_SHADER_NODE_INOUT_INCLUDE

#include<hgl/graph/shader/node/node.h>
#include<hgl/graph/shader/param/in.h>
#include<hgl/graph/shader/param/out.h>
BEGIN_SHADER_NODE_NAMESPACE
/**
 * 输入输出节点
 */
class InputOutput:public Node
{
protected:

    ObjectList<InputParam> input_params;
    ObjectList<OutputParam> output_params;

public:

    using Node::Node;
    virtual ~InputOutput()=default;
};//class InputOutput:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_INOUT_INCLUDE
