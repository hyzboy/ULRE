#pragma once

#include <cstdint>

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
    constexpr uint32_t kMeshVertexPassthroughMaxPrimitives  = 64u;
    constexpr uint32_t kMeshLineQuadMaxInvocations          = 64u;
    // CharQuad 用 TEXT_CHARQUAD_MAX_INVOCATIONS（CharQuadConfig.h——与 CPU dispatch 共享的唯一真源）
}
