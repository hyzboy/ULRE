#pragma once

#include <cstdint>

namespace hgl::graph::mtl
{
    // mesh shader 每工作组 invocation 的理想值（设备 profile 缺失/未填时的默认）。
    // 实际 group size = min(理想值, 设备能力上限)（见 GenericMaterialBuilder 的
    // ClampMeshInvocationsByDevice——拒绝在生成侧硬编码，设备上限从物理设备实测传入）。
    //
    // VertexPassthrough：96 = 3 × 32——必须是 3 的倍数（组内三角形按每 3 连续槽位
    // 装配，跨组三角形会永久丢失，见 MeshShaderAssembler 的 % 3 守卫）。
    constexpr uint32_t kMeshVertexPassthroughMaxInvocations = 96u;
    constexpr uint32_t kMeshLineQuadMaxInvocations          = 64u;
    // CharQuad 用 TEXT_CHARQUAD_MAX_INVOCATIONS（CharQuadConfig.h——与 CPU dispatch 共享的唯一真源）
}
