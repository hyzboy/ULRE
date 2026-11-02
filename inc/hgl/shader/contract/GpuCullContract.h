/// @file GpuCullContract.h
/// GPU 驱动架构与 ECS 批处理的结构常量真源。
///
/// 这些数字同时出现在 CPU 侧（`GPUCullSystem` / `EntityGPU` 的 `static constexpr`）和
/// GLSL 侧（`entity_gpu.glsl` / `entity_gpu_cull.glsl` 的 `#define`）。此前两侧各写一遍，
/// 只有回归门用**文本正则**抓取值做字符串对拍——改错一侧可能因为提取样式变化而漏检。
///
/// ⚠ 计数类（`*Count`）用「单值枚举」表达：GLSL 侧是普通 `#define`，C++ 侧读
///   `EntityGpuSlotCount::Count`。之所以不用 `static constexpr`，是因为 Clang AST JSON
///   只给 `EnumConstantDecl` 带求值结果，`VarDecl` 不带 —— 用枚举可以零歧义地取到值。

#pragma once

#include <cstdint>

#include <hgl/shader/contract/ShaderContractMacros.h>

namespace hgl::shader::contract
{
    /// 实体 GPU 槽位数（GLSL 宏 `ENTITY_GPU_SLOT_COUNT`）。
    ///
    /// `EntityGPU` 的 `type[16]` / `row[16]` 定长数组按此声明；必须 ≥ `COMPONENT_TYPE_COUNT`。
    enum class HGL_SHADER_ENUM EntityGpuSlotCount : uint32_t
    {
        Count = 16,
    };

    /// 竞技场（arena）按 pass 分扇区的份数（GLSL 宏 `ULRE_ARENA_PASSES`）。
    ///
    /// 两遍的绘制集不同 ⇒ 各写各的扇区：扇区 0 = 主帧 pass，扇区 1 = 阴影 pass。
    /// 必须与 `hgl::graph::RenderPassSlot::Count` 相等。
    enum class HGL_SHADER_ENUM ArenaPassCount : uint32_t
    {
        Passes = 2,
    };

    /// 批的 dispatch 档（随 `BatchInfoRow.topology` 下发，GLSL 宏 `TOPOLOGY_*`）。
    ///
    /// 决定 gather 阶段的线程数公式：三角形按顶点组，线按段数（每线程 1 段）。
    enum class HGL_SHADER_ENUM BatchTopology : uint32_t
    {
        Triangles = 0,   ///< 顶点组 → 三角形（VertexPassthrough）
        Lines     = 1,   ///< 段 → quad（LineQuad）
    };

    /// GPU 侧句柄哨兵（GLSL 宏 `INVALID_HANDLE`）。
    ///
    /// `TransformDataStorage` / `BoundingBoxDataStorage` 的 `INVALID_HANDLE = UINT32_MAX`，
    /// 计算着色器读父索引时必须用同一个哨兵比较。
    enum class HGL_SHADER_ENUM GpuHandleSentinel : uint32_t
    {
        InvalidHandle = 0xFFFFFFFFu,
    };
}//namespace hgl::shader::contract
