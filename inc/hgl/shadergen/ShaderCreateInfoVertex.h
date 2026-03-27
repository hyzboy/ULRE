#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

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

        int AddInput(const VAType &type,const VertexAttrib attrib);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
