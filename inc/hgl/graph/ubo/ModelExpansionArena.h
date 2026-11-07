#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/BufferAnnotation.h>
#include <cstdint>

namespace hgl::graph
{
    /**
     * GPU 模型展开竞技场（**世界私有**，地址经 WorldAddresses.addr_model_expansion 下发；
     * GLSL：#define model_expansion_arena ModelExpansionArenaRef(world_addresses.addr_model_expansion).arena）。
     *
     * 为什么独立成表而不是塞进 push constants：剔除的 push constant 已用到 112 B（设备保证
     * 下界 128 B），再塞就贴着上限走；而本表**按帧槽切份**，天然随帧滚动（展开矩阵段就是
     * 逐帧重写的），并且后续 LOD / 实例自定义数据只需追加列，无需再动管线布局。
     *
     * ## 部件 → 素材桶：为什么是 per-实体表而不是 per-部件表
     *
     * 展开侧每个部件都要落到"该部件几何所在的素材桶"里，而桶号是 **CPU 批装配期的块内
     * 编号**（着色器无从推导）。最初设计过 `uint32_t[部件池行号] → 相对槽位` 的 per-部件表，
     * 但它**不成立**：桶号是 (批, pass) 局部的，同一模板出现在两个批（或两个 pass）里时
     * 同一行会需要两个不同值 —— 一张表装不下（后写覆盖前写 = 画错几何）。
     *
     * 最终口径：相对基点从"实体标签"换成**模板素材桶组基址**，并让基址本身成为 per-实体数据
     *（本表的 `bucket_base_addr`）：
     * - CPU 装批时按 (批, 模板) 分配一段连续桶 = 该模板的**素材桶组**（组内第 i 个桶 =
     *   模板内 `bucket_slot == i` 的部件几何），并把组基址写给批内**每个**模板实体；
     * - 部件行自带的 `ModelPartRow.bucket_slot` 是**模板内常量**（按部件几何首次出现去重），
     *   与批 / pass / 帧全无关 ⇒ 不需要 per-部件表，也不需要每帧清理；
     * - 展开侧 `桶号 = bucket_base[blk][实体] + part.bucket_slot`。
     *
     * 于是同一模板跨批、跨 pass、跨帧都得到正确桶号，且同模板多实例共享同一组桶
     *（= 实例化前提：`groupCountY` 累加）。`kModelExpansionInvalidBucketBase` = 本帧没给
     * 该实体分配组（模板未打包 / 该批不接管）⇒ 展开侧整体跳过该实体。
     *
     * 语义（T3.1）：模板实体的**根变换**已在实体行里（`row[Transform]` → L2W 行号），
     * 部件世界矩阵 = 根世界矩阵 × `ModelPartRow.local_matrix`，无法用"行号"表达 ⇒ 展开侧
     * 把合成矩阵写进 L2W 池的**展开段**（紧跟动态段之后，每帧槽一份），实例行的
     * `l2w_out` 就指向段内行号。段内行号与剔除实例行号**一一对应**（`l2w_base_row + inst_row`）
     * ⇒ 无需第二张映射表。
     *
     * `flags` 位定义：位 0 = 展开分支启用（0 ⇒ `entity_gpu_cull.glsl` 走**逐字节不变**的
     * 传统单实体路径，模板事实字段被忽略 ⇒ 零回归面）。
     */
    struct alignas(16) HGL_SSBO_ROW ModelExpansionArena
    {
        uint32_t flags = 0;                     ///< 位 0 = 展开启用（其余位保留 = 0）
        uint32_t l2w_base_row = 0;              ///< 本帧槽展开矩阵段起始行号（L2W 池绝对行）
        uint32_t l2w_row_capacity = 0;          ///< 段行数；`inst_row >= capacity` ⇒ 该实例不写（防越界）
        uint32_t bucket_base_count = 0;         ///< 桶组基址表行数（= kModelExpansionMaxBucketBaseRows）

        uint64_t bucket_base_addr = 0;          ///< 桶组基址表（u32 一行，下标 = blk*每pass容量 + 实体下标）
        uint64_t reserved1 = 0;                 ///< TODO(T4.1) LOD 代理表基址
        uint64_t reserved2 = 0;                 ///< TODO(T4.4) 实例自定义数据表基址
        uint64_t reserved3 = 0;                 ///< 保留
    };

    static_assert(sizeof(ModelExpansionArena) == 48, "ModelExpansionArena 必须恰为 48 字节");

    /// 展开启用位（`flags`）
    inline constexpr uint32_t kModelExpansionFlagEnabled = 1u << 0;

    /// 槽头字节数（表体从「槽基址 + 本值」开始；与 GPUCullSystem::kExpansionHeaderBytes 同值）
    inline constexpr uint32_t kModelExpansionHeaderBytes = 64;

    /// 单个 pass 的桶组基址表容量（= GPUCullSystem::kMaxVisibleEntities，由 static_assert 锁死）
    inline constexpr uint32_t kModelExpansionBucketBaseRowsPerPass = 4096;

    /// 桶组基址表总行数上限（pass 数 × 单 pass 容量；由 static_assert 锁死）
    inline constexpr uint32_t kModelExpansionMaxBucketBaseRows = 2u * kModelExpansionBucketBaseRowsPerPass;

    /// 实体未分配模板素材桶组时的哨兵值（表初值 / CPU 允许显式写入）
    inline constexpr uint32_t kModelExpansionInvalidBucketBase = 0xFFFFFFFFu;
}
