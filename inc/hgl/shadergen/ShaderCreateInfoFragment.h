#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    FragmentShaderDescriptorInfo fsdi;

protected:

    bool ProcOutput() override;

public:

    ShaderDescriptorInfo *GetSDI()override{return &fsdi;}

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&fsdi,m);}
    ~ShaderCreateInfoFragment()=default;
    
    int AddOutput(const graph::VAType &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
    int AddOutput(const AnsiString &type,const AnsiString &name,Interpolation inter=Interpolation::Smooth);
};
}}//namespace hgl::graph