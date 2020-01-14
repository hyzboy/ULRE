#ifndef HGL_GRAPH_SHADER_NODE_OP_INCLUDE
#define HGL_GRAPH_SHADER_NODE_OP_INCLUDE

#include<hgl/graph/shader/node/node.h>
SHADER_NODE_NAMESPACE_BEGIN
enum class ScalarOpType:uint
{
    Add=0,  Sub,    Mul,    Div,
    Mod,    Pow,    Min,    Max,
    Sin,    Cos,    Atan,   Step,

    BEGIN_OP_RANGE=Add,
    END_OP_RANGE=Step,
    RANGE_SIZE=(END_OP_RANGE-BEGIN_OP_RANGE)+1
};//enum class OpType:uint

class ScalarOp:public Node
{
public:
};//class ScalarOp:public Node

class ComboVectorOp1to2:public Node
{
public:

    ComboVectorOp1to2():Node(NodeType::ComboVector)
    {
        SHADER_INPUT_PARAM(false,X,Float1)
        SHADER_INPUT_PARAM(false,Y,Float1)

        SHADER_OUTPUT_PARAM(XY,Float2)
    }
};//class ComboVectorOp1to2:public Node

enum class VectorOpType:uint
{
    Add=0,  Sub,    Mul,    Div,
    Mod,    Pow,    Min,    Max,
    Cross,  Atan,   Reflect,Step,

    BEGIN_OP_RANGE=Add,
    END_OP_RANGE=Step,
    RANGE_SIZE=(END_OP_RANGE-BEGIN_OP_RANGE)+1
};//enum class VectorOpType:uint

class VectorOp:public Node
{
public:
};//class VectorOp:public Node

enum class ColorOpType:uint
{
    Screen=0,
    Difference,
    Darken,
    Lighten,
    Overlay,
    Dodge,
    Burn,
    SoftLight,
    HardLight,

    BEGIN_OP_RANGE=Screen,
    END_OP_RANGE=HardLight,
    RANGE_SIZE=(END_OP_RANGE-BEGIN_OP_RANGE)+1
};//enum class ColorOpType:uint
SHADER_NODE_NAMESPACE_END
#endif//HGL_GRAPH_SHADER_NODE_OP_INCLUDE
