#ifndef HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE
#define HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE

#include<hgl/graph/shader/node/in.h>
#include<hgl/graph/shader/param/in.h>

BEGIN_SHADER_NODE_NAMESPACE
/**
 * 最终节点，用于最终结果的一类节点，无输出部分
 */
class Finished:public Input
{
public:

    using Input::Input;
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
    }
};//class VertexFinished:public FinishedNode

/**
 * 前向渲染最终输出节点
 */
 class ForwardFinished:public Finished
 {
 public:

    ForwardFinished():Finished("Forward Output")
    {
    }
 };//class ForwardFinished:public Finished

/**
 * GBuffer最终输出节点
 */
class GBufferFinished:public Finished
{
public:

    GBufferFinished():Finished("GBuffer Output")
    {
        SHADER_INPUT_PARAM(BaseColor,   FLOAT_3)
        SHADER_INPUT_PARAM(Normal,      FLOAT_3)
        SHADER_INPUT_PARAM(Metallic,    FLOAT_1)
        SHADER_INPUT_PARAM(Roughness,   FLOAT_1)
        SHADER_INPUT_PARAM(Opacity,     FLOAT_1)
        SHADER_INPUT_PARAM(DepthOffset, FLOAT_1)
    }
};//class GBufferFinished:public FinishedNode
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_FINISHED_INCLUDE
