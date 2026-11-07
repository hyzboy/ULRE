#pragma once
// InstanceCustomData.h —— 「逐实例个性化数据」的 ABI 定义（落地计划 T4.4，空壳）
//
// ## 为什么单独一张表而不是塞进 EntityGPU 行
//
// `EntityGPU` 行是 128 B 且已被 `E.entity-gpu-struct-parity` 台架锁死（C++ 前缀 + 尾部 pad）。
// 逐实例个性化（颜色偏移 / 材质微调 / 覆盖参数）只在**部分**实体上用，且海量实例批次
// （`GPUInstancedModelBatchComponent`）的每实例数据本来就与实体行不同源 ⇒ 独立成表，
// 用**实例行号**索引，与 `ModelExpansionArena::l2w` 展开段同一套行号（`inst_row`）。
//
// ## 索引与容量
//
// - 表基址挂在 `ModelExpansionArena::reserved2`（T4.4 启用后改名为 `custom_data_addr`；
//   当前恒 0 = 未启用 ⇒ 展开侧不读本表，行为与改造前逐字节一致）。
// - 行号 = 剔除实例行号 `inst_row`；容量与 `l2w_row_capacity` 同源（同一段行号空间），
//   `inst_row >= l2w_row_capacity` ⇒ 该实例不写自定义数据（与展开矩阵同一防越界判据）。
// - 实体想用哪一行由 `GPUModelRefComponent::custom_data_index` 指定（0 = 行 0 哨兵 =
//   引擎缺省，不读表）。
//
// ## 为什么不定义多套
//
// 16 B 是**全 GPU 侧的公共契约**（不透明地传给材质/后处理），语义由消费方约定，
// 引擎只保证「按实例行号可取到 16 B」。这与 `EntityGPU._pad_model` 的保留口径一致：
// 先固定字节数与对齐，语义后接 —— 免得每加一个特性就动一次管线布局。

#include <hgl/math/VectorTypes.h>
#include <hgl/CoreType.h>
#include <cstdint>

namespace hgl::graph::ssbo
{
    /**
     * 逐实例自定义数据行（**恰 16 字节**，对齐 16）。
     *
     * 当前（T4.4 空壳）只落一个 `custom_color`：RGB = 逐实例色调乘子，
     * A = 不透明度乘子（1.0 = 不改变）。后续如需更多逐实例量，**应**在本行内
     * 复用分量或另开表，**不得**扩大本行（16 B 是已声明的跨管线契约）。
     */
    struct alignas(16) InstanceCustomData
    {
        math::Vector4f custom_color = math::Vector4f(1.0f, 1.0f, 1.0f, 1.0f); ///< rgb 色调乘子 + a 不透明度乘子

        InstanceCustomData() = default;
        explicit InstanceCustomData(const math::Vector4f &color) : custom_color(color) {}
    };

    static_assert(sizeof(InstanceCustomData) == 16, "InstanceCustomData 必须恰为 16 字节");
    static_assert(alignof(InstanceCustomData) == 16, "InstanceCustomData 必须 alignas(16)");

    /// 「未指定自定义数据」的行号哨兵（= 行 0；与材质/几何行 0 哨兵惯例一致）
    inline constexpr uint32_t kInstanceCustomDataNone = 0u;
}
