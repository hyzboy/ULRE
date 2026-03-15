#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoCompute:public ShaderCreateInfo
    {
        ComputeShaderDescriptorInfo csdi;

    public:

        ShaderDescriptorInfo *GetSDI()override{return &csdi;}

    public:

        ShaderCreateInfoCompute(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&csdi,m);}
        ~ShaderCreateInfoCompute()override=default;

        void SetWorkGroupSize(uint32 x, uint32 y, uint32 z);
    };//class ShaderCreateInfoCompute:public ShaderCreateInfo
}//namespace hgl::graph
