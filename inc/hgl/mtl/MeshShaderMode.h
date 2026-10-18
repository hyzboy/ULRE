#pragma once

#include <hgl/CoreType.h>

#include <cstdint>
#include <cstring>

namespace hgl::graph::mtl
{
    // mesh shader 生成模式（材质 TOML [mesh_shader] mode；缺省 = VertexPassthrough）
    enum class MeshShaderMode : uint8_t
    {
        VertexPassthrough,   // 每线程 1 顶点（默认，模拟 VS）
        LineQuad,            // 每线程 1 线段 → quad
        CharQuad,            // 每线程 1 字符实例 → quad（6 顶点 2 三角形）
    };

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    inline constexpr uint32 MeshShaderModeCount =
        static_cast<uint32>(MeshShaderMode::CharQuad) + 1;

    inline const char *GetMeshShaderModeName(const MeshShaderMode mode) noexcept
    {
        switch (mode)
        {
        case MeshShaderMode::VertexPassthrough: return "VertexPassthrough";
        case MeshShaderMode::LineQuad:          return "LineQuad";
        case MeshShaderMode::CharQuad:          return "CharQuad";
        }
        return "VertexPassthrough";
    }

    // TOML 值 → 枚举。空串 = VertexPassthrough（缺省语义）；
    // 未知值返回 false（拼写错误 = 显式解析失败，而非静默降级到默认模式）。
    // 名字只在 GetMeshShaderModeName 里写一份（新增模式改枚举 + 那个 switch）。
    inline bool ParseMeshShaderMode(const char *name, MeshShaderMode &out) noexcept
    {
        if (!name || !name[0])
        {
            out = MeshShaderMode::VertexPassthrough;
            return true;
        }

        for (uint32 i = 0; i < MeshShaderModeCount; ++i)
        {
            const MeshShaderMode mode = static_cast<MeshShaderMode>(i);
            if (std::strcmp(name, GetMeshShaderModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }

    // ── 模式辅助（C4：CharQuad 特判集中于此，调用方不再散写枚举比较）──
    inline bool IsCharQuadMode(const MeshShaderMode mode) noexcept
    {
        return mode == MeshShaderMode::CharQuad;
    }

    // 每线程输出量（mesh 生成器约定）：
    // VertexPassthrough：3 顶点 1 图元（跨步协作模型）；LineQuad/CharQuad：4 顶点 2 图元（quad）
    inline uint8_t GetMeshModeVerticesPerInvocation(const MeshShaderMode mode) noexcept
    {
        if (mode == MeshShaderMode::VertexPassthrough)
            return 3u;
        return (mode == MeshShaderMode::LineQuad || mode == MeshShaderMode::CharQuad) ? 4u : 1u;
    }

    inline uint8_t GetMeshModePrimitivesPerInvocation(const MeshShaderMode mode) noexcept
    {
        if (mode == MeshShaderMode::VertexPassthrough)
            return 1u;
        return (mode == MeshShaderMode::LineQuad || mode == MeshShaderMode::CharQuad) ? 2u : 1u;
    }
}
