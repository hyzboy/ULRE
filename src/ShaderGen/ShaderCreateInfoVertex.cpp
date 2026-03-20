#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph{

ShaderCreateInfoVertex::ShaderCreateInfoVertex(MaterialDescriptorInfo *m)
    :ShaderCreateInfo(new VertexShaderDescriptorInfo(),m)
{
    vsdi=static_cast<VertexShaderDescriptorInfo *>(sdi);
}

int ShaderCreateInfoVertex::AddInput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        via.input_rate=uint8_t(VertexInputRate::Vertex);

        if(vsdi->AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const VertexAttrib attrib,const VertexInputRate input_rate)
{
    VIA via;

    via.attrib          =attrib;

    via.basetype        =(uint8) type.basetype;
    via.vec_size        =        type.vec_size;

    via.input_rate      =uint8_t(input_rate);

    via.interpolation   =Interpolation::Smooth;

    return vsdi->AddInput(via);
}
}//namespace hgl::graph

