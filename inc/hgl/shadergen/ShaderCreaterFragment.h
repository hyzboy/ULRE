#pragma once

#include<hgl/shadergen/ShaderCreater.h>

SHADERGEN_NAMESPACE_BEGIN
enum class MaterialType
{
    Color,                              ///<´¿É«²ÄÖÊ
};

class ShaderCreaterFragment:public ShaderCreater
{
    bool ProcOutput() override;

public:

    ShaderCreaterFragment(MaterialDescriptorManager *m):ShaderCreater(VK_SHADER_STAGE_FRAGMENT_BIT,m){}
    ~ShaderCreaterFragment()=default;

    void UseDefaultMain();
};
SHADERGEN_NAMESPACE_END