#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    FragmentShaderDescriptorInfo fsdi;

public:

    ShaderDescriptorInfo *GetSDI()override{return &fsdi;}

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&fsdi,m);}
    ~ShaderCreateInfoFragment()=default;
};
}}//namespace hgl::graph
