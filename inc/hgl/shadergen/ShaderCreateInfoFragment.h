#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

SHADERGEN_NAMESPACE_BEGIN
enum class MaterialType
{
    Color,                              ///<´¿É«²ÄÖÊ
};

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    bool ProcOutput() override;

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,m){}
    ~ShaderCreateInfoFragment()=default;

    void UseDefaultMain();
};
SHADERGEN_NAMESPACE_END