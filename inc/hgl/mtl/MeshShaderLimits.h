#pragma once

#include <cstdint>

#include <hgl/mtl/MeshShaderMode.h>

namespace hgl::graph::mtl
{
    // mesh shader 每工作组 invocation 的理想值（设备 profile 缺失/未填时的默认）。
    // 实际 group size = min(理想值, 设备能力上限)（见 GenericMaterialBuilder 的
    // ClampMeshInvocationsByDevice——拒绝在生成侧硬编码，设备上限从物理设备实测传入）。
    //
    // VertexPassthrough：64 线程（与 Wave32/Wave64 硬件原生对齐），
    // 采用跨步协作模型（Stride Loop）处理 192 顶点 64 三角形。
    constexpr uint32_t kMeshVertexPassthroughMaxInvocations = 64u;
    constexpr uint32_t kMeshVertexPassthroughMaxVertices    = 192u;
    constexpr uint32_t kMeshLineQuadMaxInvocations          = 64u;
    // CharQuad 用 TEXT_CHARQUAD_MAX_INVOCATIONS（CharQuadConfig.h——与 CPU dispatch 共享的唯一真源）

    struct MeshModeCapacity
    {
        uint32_t max_vertices   = 0;
        uint32_t max_primitives = 0;
    };

    // ── 「模式 × invocation 数 → 输出容量」的唯一真源 ────────────────────────
    //
    // 这个公式原先只住在 `meshgen/MeshModeDescriptor.h` 的三个 topology resolver 里，
    // 于是一旦生成器被删除，就没有任何东西能回答「固定 `.mesh` 文件里那三行
    // `layout(...)` 该是什么数值」。固定源把那三行变成**外部数据**：形状键只哈希
    // invocation 数，不哈希公式，所以手改文件里的 `local_size_x` 不会让键失配——
    // 文件照样被找到、照样编译，然后 CPU 与 shader 的组数/容量悄悄分叉。
    //
    // 因此公式搬到引擎公开头（它不含发射逻辑，只有算术），发射侧与校验侧都从这里取。
    inline MeshModeCapacity ResolveMeshModeTopology(
        const MeshShaderMode mode, const uint32_t max_invocations) noexcept
    {
        MeshModeCapacity capacity;

        switch (mode)
        {
            case MeshShaderMode::VertexPassthrough:
                // 每线程 1 顶点、跨步协作：N 线程覆盖 3N 顶点 / N 三角形。
                capacity.max_vertices   = max_invocations * 3u;
                capacity.max_primitives = max_invocations;
                break;

            case MeshShaderMode::LineQuad:
            case MeshShaderMode::CharQuad:
            default:
                // 每线程 1 线段/字符 → 1 quad（4 顶点 2 三角形）。
                capacity.max_vertices   = max_invocations * 4u;
                capacity.max_primitives = max_invocations * 2u;
                break;
        }

        return capacity;
    }
}//namespace hgl::graph::mtl
