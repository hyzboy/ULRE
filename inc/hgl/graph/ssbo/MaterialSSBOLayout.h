#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <cstddef>
#include <hgl/graph/ssbo/MaterialDataRows.h>

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

    // Arena+BDA 路径的 buffer_reference 行结构名（与 MaterialDataRows.h 的 C++ 行结构同名）
    inline const char *GetMaterialSSBORowName(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::EmissiveSurface:         return "EmissiveSurfaceRow";
        case mtl::SSBOType::TextureRectArraySurface: return "TextureRectArraySurfaceRow";
        case mtl::SSBOType::PBRSurface:              return "PBRSurfaceRow";
        case mtl::SSBOType::TransmissionSurface:     return "TransmissionSurfaceRow";
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

    // Arena+BDA 路径：行结构内纹理句柄尾（tex_tail）的字节偏移。
    // 以 C++ 行结构（MaterialDataRows.h）offsetof 为唯一真源。
    inline uint32_t GetMaterialSSBORowTexTailOffset(const mtl::SSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::SSBOType::PBRSurface:              return uint32_t(offsetof(PBRSurfaceRow,              tex_tail));
        case mtl::SSBOType::EmissiveSurface:         return uint32_t(offsetof(EmissiveSurfaceRow,         tex_tail));
        case mtl::SSBOType::TextureRectArraySurface: return uint32_t(offsetof(TextureRectArraySurfaceRow, tex_tail));
        case mtl::SSBOType::TransmissionSurface:     return uint32_t(offsetof(TransmissionSurfaceRow,     tex_tail));
        default:                                     return 0;
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
