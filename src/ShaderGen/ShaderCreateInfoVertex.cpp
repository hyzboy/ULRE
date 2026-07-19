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
        if(vsdi->AddInput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const std::string &name)
{
    return AddInput(type,GetVertexSemanticByName(name.c_str()));
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const VertexSemantic semantic)
{
    VIA via;

    via.semantic=semantic;
    hgl::strcpy(via.name,sizeof(via.name),GetVertexSemanticName(semantic));

    via.basetype=(uint8)type.basetype;
    via.vec_size=       type.vec_size;

    via.interpolation=  Interpolation::Smooth;

    return vsdi->AddInput(via);
}
}//namespace hgl::graph
