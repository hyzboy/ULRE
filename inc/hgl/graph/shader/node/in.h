#ifndef HGL_GRAPH_SHADER_NODE_INPUT_INCLUDE
#define HGL_GRAPH_SHADER_NODE_INPUT_INCLUDE

#include<hgl/graph/shader/node/node.h>
#include<hgl/graph/shader/param/in.h>

BEGIN_SHADER_NODE_NAMESPACE
/**
 * 纯输入节点
 */
class Input:public Node
{
protected:

    ObjectList<InputParam> input_params;

public:

    using Node::Node;
    virtual ~Input()=default;
};//class InputNode
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_INPUT_INCLUDE
