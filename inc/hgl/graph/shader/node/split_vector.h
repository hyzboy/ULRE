#ifndef HGL_GRAPH_SHADER_NODE_SPLIT_VECTOR_INCLUDE
#define HGL_GRAPH_SHADER_NODE_SPLIT_VECTOR_INCLUDE

#include<hgl/graph/shader/node/node.h>
BEGIN_SHADER_NODE_NAMESPACE
class SplitVector2to1:public Node
{
public:

    SplitVector2to1():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(X,Float1)
        SHADER_OUTPUT_PARAM(Y,Float1)

        SHADER_INPUT_PARAM(true,XY,Float2)
    }
};//class SplitVector2to1:public Node

class SplitVector3to1:public Node
{
public:

    SplitVector3to1():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(X,Float1)
        SHADER_OUTPUT_PARAM(Y,Float1)
        SHADER_OUTPUT_PARAM(Z,Float1)

        SHADER_INPUT_PARAM(true,XYZ,Float3)
    }
};//class SplitVector3to1:public Node

class SplitVector4to1:public Node
{
public:

    SplitVector4to1():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(X,Float1)
        SHADER_OUTPUT_PARAM(Y,Float1)
        SHADER_OUTPUT_PARAM(Z,Float1)
        SHADER_OUTPUT_PARAM(W,Float1)

        SHADER_INPUT_PARAM(true,XYZW,Float4)
    }
};//class SplitVector4to1:public Node

class SplitVector3to12:public Node
{
public:

    SplitVector3to12():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(XY,Float2)
        SHADER_OUTPUT_PARAM(Z,Float1)

        SHADER_INPUT_PARAM(true,XYZ,Float3)
    }
};//class SplitVector2to3:public Node

class SplitVector4to13:public Node
{
public:

    SplitVector4to13():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(XYZ,Float3)
        SHADER_OUTPUT_PARAM(W,Float1)

        SHADER_INPUT_PARAM(true,XYZW,Float4)
    }
};//class SplitVector4to13:public Node

class SplitVector4to22:public Node
{
public:

    SplitVector4to22():Node(NodeType::SplitVector)
    {
        SHADER_OUTPUT_PARAM(XY,Float2)
        SHADER_OUTPUT_PARAM(ZW,Float2)

        SHADER_INPUT_PARAM(true,XYZW,Float4)
    }
};//class SplitVector4to22:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_SPLIT_VECTOR_INCLUDE
