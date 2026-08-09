#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/common/VertexInputDef.h>
#include<vulkan/vulkan.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VIAArray input;

    public:

        VIAArray &GetInput(){return input;}
        const VIAArray &GetInput()const{return input;}

    public:

        ShaderCreateInfoVertex();
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);
        int AddInput(const VAType &type,const VertexSemantic semantic);
        int AddInput(const VAType &type,const std::string &name);
        int AddInput(const VkFormat format, const VertexSemantic semantic);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
