#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreaterFragment:public ShaderCreater
{
public:

    ShaderCreaterFragment():ShaderCreater(VK_SHADER_STAGE_FRAGMENT_BIT){}
    ~ShaderCreaterFragment()=default;

    void UseDefaultMain();
};
SHADERGEN_NAMESPACE_END