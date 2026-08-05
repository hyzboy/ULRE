#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph::ssbo
{
    constexpr const char EmissiveSurfaceMaterialInstanceGLSL[] = "vec4 color;";
    constexpr const char TextureRectArraySurfaceMaterialInstanceGLSL[] = "uvec4 id;";
    constexpr const char PBRSurfaceMaterialInstanceGLSL[] = R"(
        vec4  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        float fresnel;
    )";
    constexpr const char ClearCoatSurfaceMaterialInstanceGLSL[] = R"(
        vec4  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        float fresnel;
    )";
    constexpr const char TransmissionSurfaceMaterialInstanceGLSL[] = "uint TextColor;";

    inline const char *GetMaterialSSBOStructName(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:         return "EmissiveSurfaceData";
        case mtl::SSBOType::TextureRectArraySurface: return "TextureRectArraySurfaceData";
        case mtl::SSBOType::PBRSurface:              return "PBRSurfaceData";
        case mtl::SSBOType::ClearCoatSurface:        return "ClearCoatSurfaceData";
        case mtl::SSBOType::TransmissionSurface:     return "TransmissionSurfaceData";
        default:                                     return nullptr;
        }
    }

    inline const char *GetMaterialInstanceGLSL(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:     return EmissiveSurfaceMaterialInstanceGLSL;
        case mtl::SSBOType::TextureRectArraySurface: return TextureRectArraySurfaceMaterialInstanceGLSL;
        case mtl::SSBOType::PBRSurface:          return PBRSurfaceMaterialInstanceGLSL;
        case mtl::SSBOType::ClearCoatSurface:    return ClearCoatSurfaceMaterialInstanceGLSL;
        case mtl::SSBOType::TransmissionSurface: return TransmissionSurfaceMaterialInstanceGLSL;
        default:                                 return nullptr;
        }
    }

    inline bool TryGetMaterialInstanceLayout(const mtl::SSBOType type,
                                             const char *&struct_name,
                                             const char *&glsl_codes,
                                             uint32_t &struct_bytes) noexcept
    {
        struct_name = GetMaterialSSBOStructName(type);
        glsl_codes = GetMaterialInstanceGLSL(type);
        struct_bytes = mtl::GetSSBOTypeStructStride(type);
        return struct_name != nullptr && glsl_codes != nullptr && struct_bytes > 0;
    }
}
