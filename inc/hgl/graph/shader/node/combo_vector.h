#ifndef HGL_GRAPH_SHADER_NODE_COMBO_VECTOR_INCLUDE
#define HGL_GRAPH_SHADER_NODE_COMBO_VECTOR_INCLUDE

#include<hgl/graph/shader/node/node.h>
BEGIN_SHADER_NODE_NAMESPACE
class ComboVector1to2:public Node
{
    param::InputParam *ip_x,*ip_y;
    param::OutputParam *op_xy;

public:

    ComboVector1to2():Node(NodeType::ComboVector)
    {
        ip_x=SHADER_INPUT_PARAM(false,X,Float1)
        ip_y=SHADER_INPUT_PARAM(false,Y,Float1)

        op_xy=SHADER_OUTPUT_PARAM(XY,Float2)
    }
    
    bool GenOutputParamCode(UTF8StringList &)override;
    //bool GenCode(UTF8StringList &) override;
};//class ComboVector1to2:public Node

class ComboVector1to3:public Node
{
    param::InputParam *ip_x,*ip_y,*ip_z;

public:

    ComboVector1to3():Node(NodeType::ComboVector)
    {
        ip_x=SHADER_INPUT_PARAM(false,X,Float1)
        ip_y=SHADER_INPUT_PARAM(false,Y,Float1)
        ip_z=SHADER_INPUT_PARAM(false,Z,Float1)

        SHADER_OUTPUT_PARAM(XYZ,Float3)
    }

    bool GenCode(UTF8StringList &) override;
};//class ComboVector1to3:public Node

class ComboVector1to4:public Node
{
    param::InputParam *ip_x,*ip_y,*ip_z,*ip_w;

public:

    ComboVector1to4():Node(NodeType::ComboVector)
    {
        ip_x=SHADER_INPUT_PARAM(false,X,Float1)
        ip_y=SHADER_INPUT_PARAM(false,Y,Float1)
        ip_z=SHADER_INPUT_PARAM(false,Z,Float1)
        ip_w=SHADER_INPUT_PARAM(false,W,Float1)

        SHADER_OUTPUT_PARAM(XYZW,Float4)
    }

    bool GenCode(UTF8StringList &) override;
};//class ComboVector1to4:public Node

class ComboVector12to3:public Node
{
    param::InputParam *ip_xy,*ip_z;

public:

    ComboVector12to3():Node(NodeType::ComboVector)
    {
        ip_xy=SHADER_INPUT_PARAM(false,XY,Float2)
        ip_z =SHADER_INPUT_PARAM(false,Z,Float1)

        SHADER_OUTPUT_PARAM(XYZ,Float3)
    }

    bool GenCode(UTF8StringList &) override;
};//class ComboVector2to3:public Node

class ComboVector13to4:public Node
{
    param::InputParam *ip_xyz,*ip_w;

public:

    ComboVector13to4():Node(NodeType::ComboVector)
    {
        ip_xyz=SHADER_INPUT_PARAM(false,XYZ,Float3)
        ip_w  =SHADER_INPUT_PARAM(false,W,Float1)

        SHADER_OUTPUT_PARAM(XYZW,Float4)
    }

    bool GenCode(UTF8StringList &) override;
};//class ComboVector13to4:public Node

class ComboVector22to4:public Node
{
    param::InputParam *ip_xy,*ip_zw;

public:

    ComboVector22to4():Node(NodeType::ComboVector)
    {
        ip_xy=SHADER_INPUT_PARAM(false,XY,Float2)
        ip_zw=SHADER_INPUT_PARAM(false,ZW,Float2)

        SHADER_OUTPUT_PARAM(XYZW,Float4)
    }

    bool GenCode(UTF8StringList &) override;
};//class ComboVector22to4:public Node
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_COMBO_VECTOR_INCLUDE
