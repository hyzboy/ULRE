#pragma once
// GPUModelRefComponent.h —— 「这个实体引用一个 GPU 常驻模型模板」的 ECS 声明（落地计划 T2.1）
//
// 与 `ModelRefComponent`（CPU 装配请求/进度载体）不同，本组件是**渲染侧引用**：
// 模板（几何树 + 部件行）已由 `ModelTemplateManager::RegisterModelTemplate` 常驻
// VRAM 模板池，实体只持一个 `template_id`；一帧里场景多一个模型 = 实体树上加一行，
// 部件展开、LOD/阴影分流、逐部件剔除全部在 GPU 内完成（见 entity_gpu_cull.glsl）。
//
// 变换口径：本组件**不**存 TRS。实体 L2W 走常规 Transform 管线（静态挂
// `Mobility::Static`、动态挂 `Mobility::Movable`），GPU 展开时以 L2W × 部件
// local_matrix 合成部件世界矩阵，CPU 不再遍历子零件。
//
// 线程契约：只在主线程读写（与其它组件一致）。

#include <hgl/ecs/core/entity/ComponentRow.h>

namespace hgl::ecs
{
    /**
     * @brief 单实体模型引用组件（CPU-only：无 GPU 行，gpu_row_bytes = 0）。
     *
     * 全 POD 行：字段即数据，无堆持握、无虚表。可见性掩码按部件位压缩，
     * 超过 64 个部件的模板只掩码前 64 个（其余恒可见）。
     */
    struct GPUModelRefComponent : public ComponentRow
    {
        // ── 核心 ──────────────────────────────────────────────────────────
        uint32_t template_id = 0;   ///< 模板池行 ID（0 = 无效/未绑定，见 kInvalidModelTemplateID）

        // ── 可见性 ────────────────────────────────────────────────────────
        uint64_t part_visibility_mask = 0xFFFFFFFFFFFFFFFFull;  ///< 部件可见位（1 = 渲染；默认全 1）

        // ── 预留插槽（空壳，字段即 ABI，后续阶段接消费者）─────────────────
        /// 实例自定义数据行（逐实例颜色/材质微调）。
        /// 消费者：`hgl::graph::ssbo::InstanceCustomData`（见 `InstanceCustomData.h`），
        /// 行 ID 0 保留为「无自定义数据」哨兵（`kInstanceCustomDataNone`），
        /// 非 0 时按行号索引到自定义数据池，由着色器与本实体的展开实例对齐读取。
        uint32_t custom_data_index  = 0;
        uint32_t prev_transform_id  = 0;  ///< TAA motion vector 前帧变换行（T4 接）

        GPUModelRefComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；析构由池显式调用
        ~GPUModelRefComponent() = default;

        // ── 掩码操作 ──────────────────────────────────────────────────────
        void HidePart(uint32_t index)
        {
            if (index < 64u) part_visibility_mask &= ~(1ull << index);
        }

        void ShowPart(uint32_t index)
        {
            if (index < 64u) part_visibility_mask |= (1ull << index);
        }

        void ShowAllParts() { part_visibility_mask = 0xFFFFFFFFFFFFFFFFull; }

        bool IsPartVisible(uint32_t index) const
        {
            return index >= 64u || (part_visibility_mask & (1ull << index)) != 0ull;
        }
    };

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<GPUModelRefComponent> { static constexpr ComponentType value = ComponentType::GPUModelRef; };
}//namespace hgl::ecs
