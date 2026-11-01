#pragma once

#include <hgl/CoreType.h>
#include <hgl/serialize/EnumMeta.h>

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

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块（见 `EnumNameOf` / `EnumParse`）。
    // 解析口径：空串 = 缺省 `VertexPassthrough` 由调用方自行判定，未知值一律解析失败
    // （拼写错误 = 显式失败，而非静默降级到默认模式）。

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
// ── 枚举登记（拼写与 GUI 说明的唯一真源）─────────────────────────────────────

HGL_ENUM_BEGIN(hgl::graph::mtl::MeshShaderMode)
    HGL_ENUM_ITEM(VertexPassthrough, "每线程 1 顶点（默认，模拟 VS）")
    HGL_ENUM_ITEM(LineQuad,          "每线程 1 线段 → quad")
    HGL_ENUM_ITEM(CharQuad,          "每线程 1 字符实例 → quad（6 顶点 2 三角形）")
HGL_ENUM_END(hgl::graph::mtl::MeshShaderMode)