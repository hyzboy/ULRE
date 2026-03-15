#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderDescriptorInfo vsdi;

    public:

        VIAArray &GetInput(){return vsdi.GetInput();}

        ShaderDescriptorInfo *GetSDI()override{return &vsdi;}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorInfo *m):ShaderCreateInfo(){ShaderCreateInfo::Init(&vsdi,m);}
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);
        int AddInput(const VAType &type,const std::string &name,const VertexInputRate input_rate=VertexInputRate::Vertex,const VertexInputGroup &group=VertexInputGroup::Basic);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
