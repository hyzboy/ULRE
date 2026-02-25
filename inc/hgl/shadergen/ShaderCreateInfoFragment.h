#include <string>
#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{

class ShaderCreateInfoFragment:public ShaderCreateInfo
{
    FragmentShaderDescriptorInfo fsdi;

protected:

    bool IsEmptyOutput()const override{return false;/*这个返回啥无所谓，因为Fragment Shader走自己的ProcOutput*/ }
    bool ProcOutput() override;

    void AddMaterialInstanceOutput() override{};

public:

    ShaderDescriptorInfo *GetSDI()override{return &fsdi;}

public:

    ShaderCreateInfoFragment(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&fsdi,m);}
    ~ShaderCreateInfoFragment()=default;

    int AddOutput(VIAList &);
    int AddOutput(const VAType &type,const std::string &name,Interpolation inter=Interpolation::Smooth);
    int AddOutput(const std::string &type,const std::string &name,Interpolation inter=Interpolation::Smooth);
};
}}//namespace hgl::graph
