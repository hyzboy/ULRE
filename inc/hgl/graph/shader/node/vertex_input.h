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

    VertexInput():Node(NodeType::VertexInput,"VertexInput")
    {
    }

    param::OutputParam *Add(const UTF8String &n,const param::ParamType &pt)
    {
        //未来还要做个数判断之类的

        if(pt<param::ParamType::Float1
         ||pt>param::ParamType::Double4)
            return(nullptr);

        return AddOutput(n,pt);
    }

    bool GetOutputParamName(UTF8String &,const param::OutputParam *) override;
};//class VertexInput:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_VERTEX_INPUT_INCLUDE
