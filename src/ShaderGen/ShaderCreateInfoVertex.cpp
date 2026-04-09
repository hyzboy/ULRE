#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph{

ShaderCreateInfoVertex::ShaderCreateInfoVertex(MaterialDescriptorDB *m)
    :ShaderCreateInfo(new VertexShaderStageIO(),m)
{
    vsdi=static_cast<VertexShaderStageIO *>(sdi);
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

int ShaderCreateInfoVertex::AddInput(const VAType &type,const VertexAttrib attrib)
{
    return AddInput(mtl::MakeLegacyVertexAttributeSpec(type, attrib));
}

int ShaderCreateInfoVertex::AddInput(const mtl::VertexAttributeSpec &spec)
{
    VIA via{};

    via.attrib = spec.attrib;
    via.location = mtl::HasExplicitVertexLocation(spec)
                 ? spec.location
                 : static_cast<uint8>(vsdi->GetInput().count);

    via.basetype = (uint8)spec.shader_type.basetype;
    via.vec_size = spec.shader_type.vec_size;
    via.storage_format = spec.storage_format;
    via.interpolation = spec.interpolation;

    return vsdi->AddInput(via);
}
}//namespace hgl::graph

