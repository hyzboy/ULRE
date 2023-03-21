#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{
enum class MaterialType
{
    Color,                              ///<��ɫ����
};

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    bool ProcOutput() override;

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,m){}
    ~ShaderCreateInfoFragment()=default;

    void UseDefaultMain();
};
}}//namespace hgl::graph