#ifndef HGL_GRAPH_SHADER_NODE_TYPE_INCLUDE
#define HGL_GRAPH_SHADER_NODE_TYPE_INCLUDE

#include<hgl/graph/shader/common.h>

BEGIN_SHADER_NODE_NAMESPACE
enum class NodeType:int
{
    VertexInput=0,                      ///<顶点输入流节点

    ComboVector,                        ///<向量合成
    SplitVector,                        ///<向量折解

    ShaderFunction,                     ///<shader原生函数

    Const,                              ///<固定数据输入
    Param,                              ///<参数输入

    Texture,
    UBO,
    Function,

    Finished,                           ///<最终节点

    BEGIN_NODE_TYPE_RANGE   =VertexInput,
    END_NODE_TYPE_RANGE     =Finished,
    NODE_TYPE_RANGE_SIZE    =(END_NODE_TYPE_RANGE-BEGIN_NODE_TYPE_RANGE)+1
};//enum class NodeType
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_TYPE_INCLUDE
