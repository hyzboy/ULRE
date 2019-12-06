#ifndef HGL_SHADER_TEXTURE_NODE_INCLUDE
#define HGL_SHADER_TEXTURE_NODE_INCLUDE

#include<hgl/graph/shader/VectorNode.h>
BEGIN_SHADER_NODE_NAMESPACE
class texture1D:public InputOutput
{
public:

    texture1D():InputOutput("texture1D")
    {
        SHADER_INPUT_PARAM(U,FLOAT1)
    }
};//class texture1D:public InputOutput

class texture2D:public InputOutput
{
public:

    texture2D():InputOutput("texture2D")
    {
        SHADER_INPUT_PARAM(UV,FLOAT2)
    }
};//class texture2D:public InputOutput

class texture3D:public InputOutput
{
public:

    texture3D():InputOutput("texture3D")
    {
        SHADER_INPUT_PARAM(UVD,FLOAT3)
    }
};//class texture3D:public InputOutput

class TextureCube:public InputOutput
{
public:

    textureCube():InputOutput("textureCube")
    {
        SHADER_INPUT_PARAM(XYZ,FLOAT3)
    }
};//class TextureCube:public InputOutput
END_SHADER_NODE_NAMESPACE
#endif//HGL_SHADER_TEXTURE_NODE_INCLUDE
