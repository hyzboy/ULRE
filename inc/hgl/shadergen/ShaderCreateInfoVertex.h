#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<vulkan/vulkan.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderDescriptorInfo *vsdi;

    public:

        VIAArray &GetInput(){return vsdi->GetInput();}
        const VIAArray &GetInput()const{return vsdi->GetInput();}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorInfo *m);
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);
        int AddInput(const VAType &type,const VertexSemantic semantic);
        int AddInput(const VAType &type,const std::string &name);
        int AddInput(const VkFormat format, const VertexSemantic semantic);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
