#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>

namespace hgl{namespace graph{

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    bool ProcOutput() override;

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,m){}
    ~ShaderCreateInfoFragment()=default;
};
}}//namespace hgl::graph