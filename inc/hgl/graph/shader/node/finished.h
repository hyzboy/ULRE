#ifndef HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE
#define HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE

#include<hgl/graph/shader/node/node.h>
#include<hgl/graph/shader/param/in.h>

BEGIN_SHADER_NODE_NAMESPACE
/**
 * 最终节点，用于最终结果的一类节点，无输出部分
 */
class Finished:public Node
{
public:

    Finished(const UTF8String &n):Node(NodeType::Finished,n){}
    virtual ~Finished()=default;
};//class Finished:public Input

/**
 * 顶点最终输出节点
 */
class VertexFinished:public Finished
{
public:

    VertexFinished():Finished("Vertex Output")
    {
        SHADER_INPUT_PARAM(true,Position,   Float3)
    }

    ~VertexFinished()=default;
};//class VertexFinished:public FinishedNode

/**
 * 片段最终输出节点
 */
class FragmentFinished:public Finished
{
public:

    FragmentFinished():Finished("Fragment Output")
    {
        SHADER_INPUT_PARAM(false,BaseColor,   Float3)
        SHADER_INPUT_PARAM(false,Normal,      Float3)
        SHADER_INPUT_PARAM(false,Metallic,    Float1)
        SHADER_INPUT_PARAM(false,Roughness,   Float1)
        SHADER_INPUT_PARAM(false,Opacity,     Float1)
        SHADER_INPUT_PARAM(false,DepthOffset, Float1)
    }

    ~FragmentFinished()=default;
};//class FragmentFinished:public Finished
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE
