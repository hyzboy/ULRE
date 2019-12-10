#ifndef HGL_GRAPH_SHADER_NODE_OUTPUT_INCLUDE
#define HGL_GRAPH_SHADER_NODE_OUTPUT_INCLUDE

#include<hgl/graph/shader/node/node.h>
#include<hgl/graph/shader/param/out.h>
BEGIN_SHADER_NODE_NAMESPACE
/**
 * 纯输出节点，用于固定行为的向下一级节点输入数据，无输入部分
 */
class Output:public Node
{
protected:

    ObjectList<OutputParam> output_params;

public:

    using Node::Node;
    virtual ~Output()=default;
};//class Output:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_OUTPUT_INCLUDE
