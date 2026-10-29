#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <cstddef>
#include <hgl/graph/ssbo/MaterialDataRows.h>

namespace hgl::graph::ssbo
{
    // ⚠ 本文件**不再**持有任何 GLSL 文本。
    //
    // 材质内容行的 GLSL 声明现在是**派生产物**：
    //     ShaderLibrary/ssbo/pbr_surface_row.glsl
    //     ShaderLibrary/ssbo/emissive_surface_row.glsl
    //     ShaderLibrary/ssbo/transmission_surface_row.glsl
    //     ShaderLibrary/ssbo/material_data_row.glsl
    // 由 `src/Tools/BufferGen/buffer_gen.py` 从 C++ 结构 + 注解生成
    //（`buffer_gen.py --check` 在 CI 里守住漂移）。
    //
    // 这里只保留「类型 ↔ 名字 / 行结构映射」这类**纯 C++ 侧**查表。

    inline const char *GetGlobalSSBOStructName(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::EmissiveSurface:         return "EmissiveSurfaceData";
        case GlobalSSBOType::PBRSurface:              return "PBRSurfaceData";
        case GlobalSSBOType::TransmissionSurface:     return "TransmissionSurfaceData";
        default:                                     return nullptr;
        }
    }

    // Arena+BDA 路径的 buffer_reference 行结构名（与 MaterialDataRows.h 的 C++ 行结构同名）

    // 行结构类型 → GlobalSSBOType 编译期映射（GlobalSSBOBufferRegistry
    // 由 T 反查类型；新增行结构时在此登记一行）。
    template<typename T> struct GlobalRowTypeTraits;
    template<> struct GlobalRowTypeTraits<PBRSurfaceRow>              { static constexpr GlobalSSBOType TYPE = GlobalSSBOType::PBRSurface; };
    template<> struct GlobalRowTypeTraits<EmissiveSurfaceRow>         { static constexpr GlobalSSBOType TYPE = GlobalSSBOType::EmissiveSurface; };
    template<> struct GlobalRowTypeTraits<TransmissionSurfaceRow>     { static constexpr GlobalSSBOType TYPE = GlobalSSBOType::TransmissionSurface; };

    inline const char *GetGlobalSSBORowName(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::EmissiveSurface:         return "EmissiveSurfaceRow";
        case GlobalSSBOType::PBRSurface:              return "PBRSurfaceRow";
        case GlobalSSBOType::TransmissionSurface:     return "TransmissionSurfaceRow";
        default:                                     return nullptr;
        }
    }

    // GLSL buffer 声明名（struct 名去 "Data" 后缀 + "Buffer"——显式表，
    // 不做字符串剥除：改 struct 名时 buffer 名独立可控）
    inline const char *GetGlobalSSBOBufferName(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::EmissiveSurface:         return "EmissiveSurfaceBuffer";
        case GlobalSSBOType::PBRSurface:              return "PBRSurfaceBuffer";
        case GlobalSSBOType::TransmissionSurface:     return "TransmissionSurfaceBuffer";
        default:                                     return nullptr;
        }
    }

    inline bool TryGetGlobalSSBOLayout(const GlobalSSBOType type,
                                         const char *&struct_name,
                                         uint32_t &struct_bytes) noexcept
    {
        struct_name = GetGlobalSSBOStructName(type);
        struct_bytes = GetGlobalSSBOTypeStructStride(type);
        return struct_name != nullptr && struct_bytes > 0;
    }

}
