#ifndef HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE
#define HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE

#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/node.h>

BEGIN_SHADER_NODE_NAMESPACE
class Parameter:public Node
{
    param::ParamType param_type;

public:

    Parameter(const param::ParamType &pt):Node(NodeType::Param)
    {
        param_type=pt;
    }

    const param::ParamType &GetParamType()const{return param_type;}
};//class Parameter:public Node

#define SHADER_PARAMETER_CONSTRUCT_FUNC(name,value) name():Parameter(param::ParamType::name)        \
                                                    {   \
                                                        SHADER_OUTPUT_PARAM(value,name)    \
                                                    }

class Float1:public Parameter
{
    float x;

public:

    SHADER_PARAMETER_CONSTRUCT_FUNC(Float1,X)
};//class float1:public Parameter

class Float2:public Parameter
{
    float x,y;

public:

    SHADER_PARAMETER_CONSTRUCT_FUNC(Float2,XY)
};//class float2:public Parameter

class Float3:public Parameter
{
    float x,y,z;

public:

    SHADER_PARAMETER_CONSTRUCT_FUNC(Float3,XYZ)
};//class Float3:public Parameter

class Float4:public Parameter
{
    float x,y,z,w;

public:

    SHADER_PARAMETER_CONSTRUCT_FUNC(Float4,XYZW)
};//class Float4:public Parameter

#undef SHADER_PARAMETER_CONSTRUCT_FUNC
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE
