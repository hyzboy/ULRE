#ifndef HGL_GRAPH_SHADER_NODE_TEXTURE_INCLUDE
#define HGL_GRAPH_SHADER_NODE_TEXTURE_INCLUDE

#include<hgl/graph/shader/node/vector.h>
#include<hgl/graph/shader/node/node.h>
BEGIN_SHADER_NODE_NAMESPACE
class texture:public Node
{
    param::ParamType texture_type;

public:

    texture(const param::ParamType &pt,const UTF8String &n):Node(NodeType::Texture,n)
    {
        texture_type=pt;
    }

    const param::ParamType &GetTextureType()const{return texture_type;}
};//class texture:public Node

class texture1D:public texture
{
public:

    texture1D():texture(param::ParamType::Texture1D,"texture1d")
    {
        SHADER_INPUT_PARAM(false,U,Float1)
    }
};//class texture1D:public texture

class texture2D:public texture
{
public:

    texture2D():texture(param::ParamType::Texture2D,"texture2d")
    {
        SHADER_INPUT_PARAM(false,UV,Float2)
    }
};//class texture2D:public texture

class textureRect:public texture
{
public:

    textureRect():texture(param::ParamType::TextureRect,"textureRect")
    {
        SHADER_INPUT_PARAM(false,UV,UInteger2)
    }
};//class textureRect:public texture

class texture3D:public texture
{
public:

    texture3D():texture(param::ParamType::Texture3D,"texture3d")
    {
        SHADER_INPUT_PARAM(false,UVD,Float3)
    }
};//class texture3D:public texture

class textureCube:public texture
{
public:

    textureCube():texture(param::ParamType::TextureCube,"textureCube")
    {
        SHADER_INPUT_PARAM(false,XYZ,Float3)
    }
};//class TextureCube:public texture
END_SHADER_NODE_NAMESPACE
#endif//HGL_GRAPH_SHADER_NODE_TEXTURE_INCLUDE
