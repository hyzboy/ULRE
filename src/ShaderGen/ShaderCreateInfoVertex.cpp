#include<hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph::shadergen{
    using namespace hgl::graph::mtl;

ShaderCreateInfoVertex::ShaderCreateInfoVertex()
    :ShaderCreateInfo(ShaderStage::Vertex)
{}

int ShaderCreateInfoVertex::AddInput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        if(input.Add(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const std::string &name)
{
    return AddInput(type,GetVertexSemanticByName(name.c_str()));
}

namespace {
    // Convert VkFormat to {VABaseType, vec_size} for VIA construction.
    // Covers common vertex input formats; others fall back to {Float,4}.
    bool VkFormatToVATypeComponents(VkFormat fmt,
                                    VertexAttribBaseType &out_basetype,
                                    uint8_t &out_vec_size)
    {
        switch (fmt)
        {
        case VK_FORMAT_R8_UNORM:               out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R8G8_UNORM:             out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R8G8B8A8_UNORM:         out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R8_SNORM:               out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R8G8_SNORM:             out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R8G8B8A8_SNORM:         out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R8_UINT:                out_basetype=VertexAttribBaseType::UInt;   out_vec_size=1; return true;
        case VK_FORMAT_R8G8_UINT:              out_basetype=VertexAttribBaseType::UInt;   out_vec_size=2; return true;
        case VK_FORMAT_R8G8B8A8_UINT:          out_basetype=VertexAttribBaseType::UInt;   out_vec_size=4; return true;
        case VK_FORMAT_R8_SINT:                out_basetype=VertexAttribBaseType::Int;    out_vec_size=1; return true;
        case VK_FORMAT_R8G8_SINT:              out_basetype=VertexAttribBaseType::Int;    out_vec_size=2; return true;
        case VK_FORMAT_R8G8B8A8_SINT:          out_basetype=VertexAttribBaseType::Int;    out_vec_size=4; return true;
        case VK_FORMAT_R16_UNORM:              out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R16G16_UNORM:           out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R16G16B16A16_UNORM:     out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R16_SNORM:              out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R16G16_SNORM:           out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R16G16B16A16_SNORM:     out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R16_SFLOAT:             out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R16G16_SFLOAT:          out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R16G16B16A16_SFLOAT:    out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R32_SFLOAT:              out_basetype=VertexAttribBaseType::Float;  out_vec_size=1; return true;
        case VK_FORMAT_R32G32_SFLOAT:           out_basetype=VertexAttribBaseType::Float;  out_vec_size=2; return true;
        case VK_FORMAT_R32G32B32_SFLOAT:        out_basetype=VertexAttribBaseType::Float;  out_vec_size=3; return true;
        case VK_FORMAT_R32G32B32A32_SFLOAT:     out_basetype=VertexAttribBaseType::Float;  out_vec_size=4; return true;
        case VK_FORMAT_R32_SINT:                out_basetype=VertexAttribBaseType::Int;    out_vec_size=1; return true;
        case VK_FORMAT_R32G32_SINT:             out_basetype=VertexAttribBaseType::Int;    out_vec_size=2; return true;
        case VK_FORMAT_R32G32B32_SINT:          out_basetype=VertexAttribBaseType::Int;    out_vec_size=3; return true;
        case VK_FORMAT_R32G32B32A32_SINT:       out_basetype=VertexAttribBaseType::Int;    out_vec_size=4; return true;
        case VK_FORMAT_R32_UINT:                out_basetype=VertexAttribBaseType::UInt;   out_vec_size=1; return true;
        case VK_FORMAT_R32G32_UINT:             out_basetype=VertexAttribBaseType::UInt;   out_vec_size=2; return true;
        case VK_FORMAT_R32G32B32_UINT:          out_basetype=VertexAttribBaseType::UInt;   out_vec_size=3; return true;
        case VK_FORMAT_R32G32B32A32_UINT:       out_basetype=VertexAttribBaseType::UInt;   out_vec_size=4; return true;
        // 16-bit int/uint formats
        case VK_FORMAT_R16_SINT:                out_basetype=VertexAttribBaseType::Int;    out_vec_size=1; return true;
        case VK_FORMAT_R16G16_SINT:             out_basetype=VertexAttribBaseType::Int;    out_vec_size=2; return true;
        case VK_FORMAT_R16G16B16A16_SINT:       out_basetype=VertexAttribBaseType::Int;    out_vec_size=4; return true;
        case VK_FORMAT_R16_UINT:                out_basetype=VertexAttribBaseType::UInt;   out_vec_size=1; return true;
        case VK_FORMAT_R16G16_UINT:             out_basetype=VertexAttribBaseType::UInt;   out_vec_size=2; return true;
        case VK_FORMAT_R16G16B16A16_UINT:       out_basetype=VertexAttribBaseType::UInt;   out_vec_size=4; return true;
        default:
            out_basetype=VertexAttribBaseType::Float;
            out_vec_size=4;
            return false;
        }
    }
}

int ShaderCreateInfoVertex::AddInput(const VkFormat format, const VertexSemantic semantic)
{
    VertexAttribBaseType basetype;
    uint8_t vec_size;
    VkFormatToVATypeComponents(format, basetype, vec_size);

    VAType va_type;
    va_type.basetype = basetype;
    va_type.vec_size = vec_size;

    return AddInput(va_type, semantic);
}

int ShaderCreateInfoVertex::AddInput(const VAType &type,const VertexSemantic semantic)
{
    VIA via;

    via.semantic=semantic;
    hgl::strcpy(via.name,sizeof(via.name),GetVertexSemanticName(semantic));

    via.basetype=(uint8)type.basetype;
    via.vec_size=       type.vec_size;

    via.interpolation=  Interpolation::Smooth;

    return input.Add(via);
}
}//namespace hgl::graph::shadergen
