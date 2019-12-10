#ifndef HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE
#define HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE

#include<hgl/graph/shader/param/out.h>
#include<hgl/graph/shader/node/out.h>

BEGIN_SHADER_NODE_NAMESPACE
class float1:public Output
{
    float x;

public:

    float1():Output("float1")
    {
        SHADER_OUTPUT_PARAM(X,FLOAT_1)
    }
};//class float1:public Output

class float2:public Output
{
    float x,y;

public:

    float2():Output("float2")
    {
        SHADER_OUTPUT_PARAM(XY,FLOAT_2)
    }
};//class float2:public Output

class float3:public Output
{
    float x,y,z;

public:

    float3():Output("float3")
    {
        SHADER_OUTPUT_PARAM(XYZ,FLOAT_3)
    }
};//class float3:public Output

class float4:public Output
{
    float x,y,z,w;

public:

    float4():Output("float4")
    {
        SHADER_OUTPUT_PARAM(XYZW,FLOAT_4)
    }
};//class float4:public Output
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_VECTOR_INCLUDE
