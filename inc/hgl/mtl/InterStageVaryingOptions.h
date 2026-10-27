#pragma once

#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    // ═══════════════════════════════════════════════════════════════════════════
    // common_options.glsl —— 阶段间 varying 的唯一开关源
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 网格阶段与片段阶段之间传什么数据，原先由**两处独立的事实**决定：
    //   · 网格侧：C++ 按 `MaterialVertexVaryingConfig` 拼出形状名（
    //     `tri_192_dataid_pos_nrm_uv` 之类），再挑一份形状主着色器；
    //   · 片段侧：各领域 `.frag` 主着色器里手抄一份 `layout(location=...) in`。
    // 两个集合必须逐 location 相等，而它们之间没有任何机械约束——只能靠人。
    //
    // 现在两端都从本文件发出去的宏里取开关：
    //
    //   `ShaderLibrary/common/interstage_varying.glsl` 里每条语义形如
    //     #if defined(HGL_VARYING_WORLD_POS)
    //     layout(location=HGL_LOCATION_WORLD_POS) out vec3 fragWorldPos[N];   // mesh
    //     layout(location=HGL_LOCATION_WORLD_POS) in  vec3 fragWorldPos;      // frag
    //     #endif
    //   一个形状主着色器、一个宏集合 ⇒ 接口天然 100% 匹配，不可能出现
    //   「多一个/少一个 location」的管线不匹配。
    //
    // 三条硬约束（与 frag_options.glsl / mesh_options.glsl 同源）：
    //
    //   1. **只发射被置位的语义**。宏名由 `InterStageSemanticRegistry[]` 的
    //      `location_macro` 派生（`HGL_LOCATION_WORLD_POS` → `HGL_VARYING_WORLD_POS`），
    //      不在本文件维护第二份对照表。
    //   2. **vertex color 恒发**。网格侧不区分「直出 / 调色板 / 缺省白」三种
    //      顶点色来源，一律输出顶点色；`BuildMeshShapeName` 也是这么算形状名的
    //      （非 CharQuad 模式一律 `mask &= ~Color`，再在形状主着色器里无条件写出）。
    //      把 Color 纳入领域契约需要改 14 条 `DomainContracts[]`，收益为零。
    //   3. **输出无注释无空行**——这段文本进 options 键（`MakeOptionsKey`），
    //      是 program 身份的一部分。
    //
    // 本函数**不看**顶点格式，也不判断「顶点流里有没有法线」：法线有无是
    // `mesh_options.glsl` 的事（没有 Normal 属性时 `GetWorldNormal` 根本不存在，
    // 网格阶段会在 GLSL 里直接编不过）。若把那条判据也搬进来，同一件事就有了
    // 两个判据，而它们对「领域契约要求 WorldNormal、材质自己没声明法线需求」
    // 这类组合的结论必然不同。

    /// 依阶段间语义 mask 发射 `HGL_VARYING_*` 开关。
    ///
    /// @param mask     本 pass 的阶段间语义集合（领域契约 ∩ 覆盖需求），
    ///                 通常来自 `GetMaterialInterStageSemanticMask(varying)`
    /// @param out_text 成功后为完整 options 文本；失败时保持未定义内容
    /// @return 成功返回 true
    bool EmitInterStageVaryingOptions(InterStageSemanticMask mask,
                                      AnsiString &out_text);
}// namespace hgl::graph::mtl
