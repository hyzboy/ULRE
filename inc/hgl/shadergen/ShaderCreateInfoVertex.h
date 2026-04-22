#pragma once

#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>

namespace hgl::graph
{
    class ShaderCreateInfoVertex:public ShaderCreateInfo
    {
        VertexShaderStageIO *vertex_stage_io;

    public:

        VIAArray &GetInput(){return vertex_stage_io->GetInput();}
        const VIAArray &GetInput()const{return vertex_stage_io->GetInput();}

    public:

        ShaderCreateInfoVertex(MaterialDescriptorDB *m);
        ~ShaderCreateInfoVertex()override=default;

        int AddInput(VIAList &);

        int AddInput(const VAType &type,const VertexAttrib attrib);
    };//class ShaderCreateInfoVertex:public ShaderCreateInfo
}//namespace hgl::graph
