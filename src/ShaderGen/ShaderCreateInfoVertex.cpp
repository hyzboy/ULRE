#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph{

int ShaderCreateInfoVertex::AddInput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        via.input_rate=uint8_t(VertexInputRate::Vertex);
        via.group=VertexInputGroup::Basic;

        if(vsdi.AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const std::string &name,const VertexInputRate input_rate,const VertexInputGroup &group)
{
    VIA via;

    hgl::strcpy(via.name,sizeof(via.name),name.c_str());

    via.basetype=(uint8) type.basetype;
    via.vec_size=        type.vec_size;

    via.input_rate      =uint8_t(input_rate);
    via.group           =group;

    via.interpolation   =Interpolation::Smooth;

    return vsdi.AddInput(via);
}
}//namespace hgl::graph

