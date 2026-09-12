#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <cstddef>
#include <hgl/graph/ssbo/MaterialDataRows.h>

namespace hgl::graph::ssbo
{
    constexpr const char EmissiveSurfaceMaterialSSBOGLSL[] = "vec4 color;";
    constexpr const char PBRSurfaceMaterialSSBOGLSL[] = R"(
        vec4  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        float fresnel;
    )";
    constexpr const char TransmissionSurfaceMaterialSSBOGLSL[] = "uint trans_color; uint reserved0[3];";

    inline const char *GetMaterialSSBOStructName(const mtl::MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::MaterialSSBOType::EmissiveSurface:         return "EmissiveSurfaceData";
        case mtl::MaterialSSBOType::PBRSurface:              return "PBRSurfaceData";
        case mtl::MaterialSSBOType::TransmissionSurface:     return "TransmissionSurfaceData";
        default:                                     return nullptr;
        }
    }

    // Arena+BDA 路径的 buffer_reference 行结构名（与 MaterialDataRows.h 的 C++ 行结构同名）

    // 行结构类型 → MaterialSSBOType 编译期映射（MaterialSSBOBufferRegistry
    // 由 T 反查类型；新增行结构时在此登记一行）。
    template<typename T> struct MaterialRowTypeTraits;
    template<> struct MaterialRowTypeTraits<PBRSurfaceRow>              { static constexpr mtl::MaterialSSBOType TYPE = mtl::MaterialSSBOType::PBRSurface; };
    template<> struct MaterialRowTypeTraits<EmissiveSurfaceRow>         { static constexpr mtl::MaterialSSBOType TYPE = mtl::MaterialSSBOType::EmissiveSurface; };
    template<> struct MaterialRowTypeTraits<TransmissionSurfaceRow>     { static constexpr mtl::MaterialSSBOType TYPE = mtl::MaterialSSBOType::TransmissionSurface; };

    inline const char *GetMaterialSSBORowName(const mtl::MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::MaterialSSBOType::EmissiveSurface:         return "EmissiveSurfaceRow";
        case mtl::MaterialSSBOType::PBRSurface:              return "PBRSurfaceRow";
        case mtl::MaterialSSBOType::TransmissionSurface:     return "TransmissionSurfaceRow";
        default:                                     return nullptr;
        }
    }

    // GLSL buffer 声明名（struct 名去 "Data" 后缀 + "Buffer"——显式表，
    // 不做字符串剥除：改 struct 名时 buffer 名独立可控）
    inline const char *GetMaterialSSBOBufferName(const mtl::MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::MaterialSSBOType::EmissiveSurface:         return "EmissiveSurfaceBuffer";
        case mtl::MaterialSSBOType::PBRSurface:              return "PBRSurfaceBuffer";
        case mtl::MaterialSSBOType::TransmissionSurface:     return "TransmissionSurfaceBuffer";
        default:                                     return nullptr;
        }
    }

    inline const char *GetMaterialSSBOStructGLSL(const mtl::MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case mtl::MaterialSSBOType::EmissiveSurface:     return EmissiveSurfaceMaterialSSBOGLSL;
        case mtl::MaterialSSBOType::PBRSurface:              return PBRSurfaceMaterialSSBOGLSL;
        case mtl::MaterialSSBOType::TransmissionSurface:     return TransmissionSurfaceMaterialSSBOGLSL;
        default:                                 return nullptr;
        }
    }

    inline bool TryGetMaterialSSBOLayout(const mtl::MaterialSSBOType type,
                                         const char *&struct_name,
                                         const char *&glsl_codes,
                                         uint32_t &struct_bytes) noexcept
    {
        struct_name = GetMaterialSSBOStructName(type);
        glsl_codes = GetMaterialSSBOStructGLSL(type);
        struct_bytes = mtl::GetMaterialSSBOTypeStructStride(type);
        return struct_name != nullptr && glsl_codes != nullptr && struct_bytes > 0;
    }

}
