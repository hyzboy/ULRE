#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph::ssbo
{
    constexpr const char EmissiveSurfaceMaterialSSBOGLSL[] = "vec4 color;";
    constexpr const char TextureRectArraySurfaceMaterialSSBOGLSL[] = "uvec4 id;";
    constexpr const char PBRSurfaceMaterialSSBOGLSL[] = R"(
        vec4  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        float fresnel;
    )";
    constexpr const char ClearCoatSurfaceMaterialSSBOGLSL[] = R"(
        vec4  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        float fresnel;
    )";
    constexpr const char TransmissionSurfaceMaterialSSBOGLSL[] = "uint TextColor;";

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

    inline const char *GetMaterialSSBOStructGLSL(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:     return EmissiveSurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::TextureRectArraySurface: return TextureRectArraySurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::PBRSurface:          return PBRSurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::ClearCoatSurface:    return ClearCoatSurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::TransmissionSurface: return TransmissionSurfaceMaterialSSBOGLSL;
        default:                                 return nullptr;
        }
    }

    inline bool TryGetMaterialSSBOLayout(const mtl::SSBOType type,
                                         const char *&struct_name,
                                         const char *&glsl_codes,
                                         uint32_t &struct_bytes) noexcept
    {
        struct_name = GetMaterialSSBOStructName(type);
        glsl_codes = GetMaterialSSBOStructGLSL(type);
        struct_bytes = mtl::GetSSBOTypeStructStride(type);
        return struct_name != nullptr && glsl_codes != nullptr && struct_bytes > 0;
    }
}
