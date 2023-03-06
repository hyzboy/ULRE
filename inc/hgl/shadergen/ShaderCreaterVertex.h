#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreaterVertex:public ShaderCreater
{
public:

    ShaderCreaterVertex():ShaderCreater(VK_SHADER_STAGE_VERTEX_BIT){}
    ~ShaderCreaterVertex()=default;

    void UseDefaultMain();
};
SHADERGEN_NAMESPACE_END