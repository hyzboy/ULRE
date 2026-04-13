#pragma once

#include <hgl/common/InterpolationDef.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/FixedVertexEntry.h>
#include <hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

constexpr uint8 AUTO_VERTEX_ATTRIBUTE_LOCATION = 0xFF;

struct VertexAttributeSpec
{
    VertexAttrib         attrib;
    VAType               shader_type;
    VkFormat             storage_format = VK_FORMAT_UNDEFINED;
    uint8                location = AUTO_VERTEX_ATTRIBUTE_LOCATION;
    VkVertexInputRate    input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    Interpolation        interpolation = Interpolation::Smooth;
};

inline bool IsStorageFormatCompatibleWithShaderType(const VAType &shader_type,const VkFormat storage_format,const VertexAttrib attrib=VertexAttrib::RANGE_SIZE)
{
    if(!shader_type.Check())
        return false;

    if(storage_format == VK_FORMAT_UNDEFINED)
        return true;

    const uint8 n = shader_type.vec_size;

    // Special handling for Normal and Tangent attributes: allow 2/3/4 channel formats
    // These are direction vectors that can be stored in various compressed formats
    if((attrib == VertexAttrib::Normal || attrib == VertexAttrib::Tangent) && shader_type.basetype == VABaseType::Float)
    {
        // Support common direction vector storage formats regardless of shader vec_size
        // This allows R8G8_SNORM (2ch), RGB16F (3ch), RGBA8_SNORM (4ch), etc.
        switch(storage_format)
        {
            // 2-channel formats
            case PF_RG8UN:
            case PF_RG8SN:
            case PF_RG16UN:
            case PF_RG16F:
            case PF_RG32F:
            // 3-channel formats
            case PF_RGB16F:
            case PF_RGB32F:
            // 4-channel formats
            case PF_RGBA8UN:
            case PF_RGBA8SN:
            case PF_A2RGB10SN:
            case PF_A2BGR10SN:
            case PF_RGBA16UN:
            case PF_RGBA16F:
            case PF_RGBA32F:
                return true;
            default:
                break;
        }
    }

    switch(shader_type.basetype)
    {
        case VABaseType::Float:
            switch(n)
            {
                case 1: return storage_format==PF_R8UN
                              ||storage_format==PF_R8SN
                              ||storage_format==PF_R16UN
                              ||storage_format==PF_R16F
                              ||storage_format==PF_R32F;
                case 2: return storage_format==PF_RG8UN
                              ||storage_format==PF_RG8SN
                              ||storage_format==PF_RG16UN
                              ||storage_format==PF_RG16F
                              ||storage_format==PF_RG32F;
                case 3: return storage_format==PF_RGB16F
                              ||storage_format==PF_RGB32F;
                case 4: return storage_format==PF_RGB16F
                              ||storage_format==PF_RGB32F
                              ||storage_format==PF_RGBA8UN
                              ||storage_format==PF_RGBA8SN
                              ||storage_format==PF_A2RGB10SN
                              ||storage_format==PF_A2BGR10SN
                              ||storage_format==PF_RGBA16UN
                              ||storage_format==PF_RGBA16F
                              ||storage_format==PF_RGBA32F;
            }
            break;

        case VABaseType::Int:
            switch(n)
            {
                case 1: return storage_format==PF_R8I
                              ||storage_format==PF_R16I
                              ||storage_format==PF_R32I;
                case 2: return storage_format==PF_RG8I
                              ||storage_format==PF_RG16I
                              ||storage_format==PF_RG32I;
                case 3: return storage_format==PF_RGB16I
                              ||storage_format==PF_RGB32I;
                case 4: return storage_format==PF_RGBA8I
                              ||storage_format==PF_RGBA16I
                              ||storage_format==PF_RGBA32I;
            }
            break;

        case VABaseType::UInt:
            switch(n)
            {
                case 1: return storage_format==PF_R8U
                              ||storage_format==PF_R16U
                              ||storage_format==PF_R32U;
                case 2: return storage_format==PF_RG8U
                              ||storage_format==PF_RG16U
                              ||storage_format==PF_RG32U;
                case 3: return storage_format==PF_RGB16U
                              ||storage_format==PF_RGB32U;
                case 4: return storage_format==PF_RGBA8U
                              ||storage_format==PF_RGBA16U
                              ||storage_format==PF_RGBA32U;
            }
            break;

        case VABaseType::Double:
            switch(n)
            {
                case 1: return storage_format==PF_R64F;
                case 2: return storage_format==PF_RG64F;
                case 3: return storage_format==PF_RGB64F;
                case 4: return storage_format==PF_RGBA64F;
            }
            break;

        case VABaseType::Bool:
            // Vulkan vertex input has no bool format; keep legacy mapping to uint formats.
            switch(n)
            {
                case 1: return storage_format==PF_R8U
                              ||storage_format==PF_R16U
                              ||storage_format==PF_R32U;
                case 2: return storage_format==PF_RG8U
                              ||storage_format==PF_RG16U
                              ||storage_format==PF_RG32U;
                case 4: return storage_format==PF_RGBA8U
                              ||storage_format==PF_RGBA16U
                              ||storage_format==PF_RGBA32U;
            }
            break;

        case VABaseType::UByteNorm:
            // Legacy compat type. Shader-side should be float* with UNORM storage.
            switch(n)
            {
                case 1: return storage_format==PF_R8UN;
                case 2: return storage_format==PF_RG8UN;
                case 4: return storage_format==PF_RGBA8UN;
            }
            break;
    }

    return false;
}

inline bool ValidateVertexAttributeSpec(const VertexAttributeSpec &spec)
{
    if(!RangeCheck(spec.attrib))
        return false;

    if(!spec.shader_type.Check())
        return false;

    return IsStorageFormatCompatibleWithShaderType(spec.shader_type,spec.storage_format,spec.attrib);
}

inline bool HasExplicitVertexLocation(const VertexAttributeSpec &spec)
{
    return spec.location != AUTO_VERTEX_ATTRIBUTE_LOCATION;
}

// DEPRECATED: Converts legacy VAType-based pair to modern VertexAttributeSpec.
// Should only be used as a bridge for backwards compatibility.
[[deprecated("This is a legacy bridge function. New code should construct VertexAttributeSpec directly with explicit storage_format.")]]
inline VertexAttributeSpec MakeLegacyVertexAttributeSpec(const VAType &type,const VertexAttrib attrib)
{
    VertexAttributeSpec spec{};

    spec.attrib = attrib;
    spec.shader_type = type;
    spec.storage_format = GetVulkanFormat(&type);

    return spec;
}

inline VertexAttributeSpec MakeLegacyVertexAttributeSpec(const FixedVertexEntry &entry)
{
    return MakeLegacyVertexAttributeSpec(entry.type, entry.attrib);
}

}//namespace hgl::graph::mtl
