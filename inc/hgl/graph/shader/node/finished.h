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

    using Node::Node;
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
        SHADER_INPUT_PARAM(Position,   Float3)
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
        SHADER_INPUT_PARAM(BaseColor,   Float3)
        SHADER_INPUT_PARAM(Normal,      Float3)
        SHADER_INPUT_PARAM(Metallic,    Float1)
        SHADER_INPUT_PARAM(Roughness,   Float1)
        SHADER_INPUT_PARAM(Opacity,     Float1)
        SHADER_INPUT_PARAM(DepthOffset, Float1)
    }

    ~FragmentFinished()=default;
};//class FragmentFinished:public Finished
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE
