#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoGeometry:public ShaderCreateInfo
    {
        GeometryShaderDescriptorInfo gsdi;

    public:

        ShaderDescriptorInfo *GetSDI()override{return &gsdi;}

    public:

        ShaderCreateInfoGeometry(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&gsdi,m);}
        ~ShaderCreateInfoGeometry()override=default;
    };
}//namespace hgl::graph
