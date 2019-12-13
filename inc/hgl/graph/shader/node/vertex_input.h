#ifndef HGL_GRAPH_SHADER_NODE_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_SHADER_NODE_VERTEX_INPUT_INCLUDE

#include<hgl/graph/shader/node/node.h>
BEGIN_SHADER_NODE_NAMESPACE
/**
 * 顶点输入流节点
 */
class VertexInput:public Node
{
public:

    VertexInput(const UTF8String &n,const param::ParamType &pt):Node("VertexInput",n)
    {
        AddOutput(n,pt);
    }
};//class VertexInput:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_VERTEX_INPUT_INCLUDE
