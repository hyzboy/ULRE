#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph{

ShaderCreateInfoVertex::ShaderCreateInfoVertex(MaterialDescriptorDB *m)
    :ShaderCreateInfo(new VertexShaderStageIO(),m)
{
    vertex_stage_io=static_cast<VertexShaderStageIO *>(stage_io);
}

int ShaderCreateInfoVertex::AddInput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        if(vertex_stage_io->AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const VertexAttrib attrib)
{
    VIA via;

    via.attrib          =attrib;

    via.basetype        =(uint8) type.basetype;
    via.vec_size        =        type.vec_size;

    via.interpolation   =Interpolation::Smooth;

    return vertex_stage_io->AddInput(via);
}
}//namespace hgl::graph

