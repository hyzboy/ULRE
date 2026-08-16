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
    constexpr const char TransmissionSurfaceMaterialSSBOGLSL[] = "uint TextColor;";

    inline const char *GetMaterialSSBOStructName(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:         return "EmissiveSurfaceData";
        case mtl::SSBOType::TextureRectArraySurface: return "TextureRectArraySurfaceData";
        case mtl::SSBOType::PBRSurface:              return "PBRSurfaceData";
        case mtl::SSBOType::TransmissionSurface:     return "TransmissionSurfaceData";
        default:                                     return nullptr;
        }
    }

    // GLSL buffer 声明名（struct 名去 "Data" 后缀 + "Buffer"——显式表，
    // 不做字符串剥除：改 struct 名时 buffer 名独立可控）
    inline const char *GetMaterialSSBOBufferName(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:         return "EmissiveSurfaceBuffer";
        case mtl::SSBOType::TextureRectArraySurface: return "TextureRectArraySurfaceBuffer";
        case mtl::SSBOType::PBRSurface:              return "PBRSurfaceBuffer";
        case mtl::SSBOType::TransmissionSurface:     return "TransmissionSurfaceBuffer";
        default:                                     return nullptr;
        }
    }

    inline const char *GetMaterialSSBOStructGLSL(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:     return EmissiveSurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::TextureRectArraySurface: return TextureRectArraySurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::PBRSurface:              return PBRSurfaceMaterialSSBOGLSL;
        case mtl::SSBOType::TransmissionSurface:     return TransmissionSurfaceMaterialSSBOGLSL;
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
