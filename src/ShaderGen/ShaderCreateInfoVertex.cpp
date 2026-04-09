#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph{

ShaderCreateInfoVertex::ShaderCreateInfoVertex(MaterialDescriptorDB *m)
    :ShaderCreateInfo(new VertexShaderStageIO(),m)
{
    vsdi=static_cast<VertexShaderStageIO *>(sdi);
}

// Legacy path: Direct VIA list input bypasses ValidateVertexAttributeSpec() checks.
// This exists only for backwards compatibility; new call sites should use AddInput(VertexAttributeSpec).
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
    if(!mtl::ValidateVertexAttributeSpec(spec))
        return -2;

    VIA via{};

    via.attrib = spec.attrib;
    via.location = mtl::HasExplicitVertexLocation(spec)
                 ? spec.location
                 : static_cast<uint8>(vsdi->GetInput().count);

    via.basetype = (uint8)spec.shader_type.basetype;
    via.vec_size = spec.shader_type.vec_size;
    via.storage_format = spec.storage_format;
    via.interpolation = spec.interpolation;

    return(vsdi->AddInput(via)?1:-1);
}

int ShaderCreateInfoVertex::AddInput(const mtl::VertexAttributeSpec *specs, uint count)
{
    if(!specs || count == 0)
        return 0;

    int total = 0;

    for(uint i = 0; i < count; ++i)
    {
        int result = AddInput(specs[i]);
        if(result > 0)
            total += result;
        else if(result < 0)
            return result;  // Pass through validation failure
    }

    return total;
}
}//namespace hgl::graph

