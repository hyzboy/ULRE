#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/mtl/VertexAttributeSpec.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderStageIO *vsdi;

    public:

        VIAArray &GetInput(){return vsdi->GetInput();}
        const VIAArray &GetInput()const{return vsdi->GetInput();}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorDB *m);
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);

        int AddInput(const VAType &type,const VertexAttrib attrib);
        int AddInput(const mtl::VertexAttributeSpec &spec);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
