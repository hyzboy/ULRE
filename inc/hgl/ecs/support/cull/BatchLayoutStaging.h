#pragma once

#include<cstdint>

namespace hgl::ecs
{
    /**
     * 批分区装配的 CPU 侧暂存（GPU 剔除系统私有）。
     *
     * 与设备缓冲的 Map/Unmap、队列等待**彻底分开**：本类只管"每 pass 一块批行表 +
     * 本帧批数"这份纯 CPU 状态与其校验（批号/批数越界 fail-fast、切片起点 4 行对齐契约）。
     *
     * 分区与偏移的唯一权威在本类：块基址 `pass * kBlockStride`、行字段顺序、批数按 pass
     * 分开存放（共用一份会让一帧内后一次装配的批数作用到两个 pass —— 主帧批被整批丢弃）。
     *
     * 行布局 `Row` 与 `entity_gpu_cull.glsl` 的 `BatchInfoRow` **逐字段同序同型**（16 B/行）。
     */
    class BatchLayoutStaging
    {
    public:
        /// 按 pass 分份的遍数（与 `GPUCullSystem::kArenaPasses` / GLSL `ULRE_ARENA_PASSES` 同值）
        static constexpr uint32_t kPasses = 2;

        /// 批数上限 = **素材桶（geometry bucket）数**上限（每 材质批 × 几何体 组合占一行）。
        /// 超限 = 项目 bug ⇒ fail-fast，不扩容、不静默丢批。
        static constexpr uint32_t kMaxBatches = 1024;

        static constexpr uint32_t kInvalidTag = 0xFFFFFFFFu;   ///< 不属任何批 ⇒ cull 跳过

        /// 批数是否在上限内（上限的唯一判定口径；越界 = 项目 bug ⇒ 调用方 fail-fast）
        static constexpr bool IsValidBatchCount(uint32_t count) { return count <= kMaxBatches; }

        /// 块内行数 = 批行 + 1 个溢出槽（与 GLSL 溢出槽口径一致）
        static constexpr uint32_t kBlockStride = kMaxBatches + 1;

        /// 批的 dispatch 档（与 GLSL 的 `TOPOLOGY_*` 同步）：决定 gather 的 X 公式。
        static constexpr uint32_t kTopologyTriangles = 0;  ///< 顶点组→三角形
        static constexpr uint32_t kTopologyLines     = 1;  ///< 段→quad（每线程 1 段）

        /// 批信息行（与 `entity_gpu_cull.glsl` 的 `BatchInfoRow` 逐字段同序同型；16 B/行）
        struct Row
        {
            uint32_t row_base;    ///< 本批切片在共享竞技场里的起始行
            uint32_t capacity;    ///< 切片容量（= 本批 item 数；绘制侧 maxDrawCount）
            uint32_t topology;    ///< kTopology*
            uint32_t group_size;  ///< MeshDispatchGroupSize(is_lines)（CPU 单源下发）
        };

        /// 块基址：块 0 = 主帧 pass、块 1 = 阴影 pass
        static constexpr uint32_t BlockBase(uint32_t pass_index)
        {
            return (pass_index < kPasses ? pass_index : 0u) * kBlockStride;
        }

        /// 切片起点对齐契约（§9 M4）：三表 push 地址 = 基址 + `row_base × 行字节`
        /// （cmds 8B / l2w 4B / mtl 16B），而引用声明 16B 对齐 ⇒ `row_base % 4 == 0` 是
        /// 同时满足三者的唯一条件。
        static constexpr uint32_t kSliceRowAlignment = 4;

    private:
        Row rows[kPasses * kBlockStride] = {};
        /// 本帧批数，**按 pass 分开**（0 = 无批）
        uint32_t counts[kPasses] = {};

        bool reported_misaligned_rows = false;

    public:
        /// 进入某 pass 的装配：**必须每帧清本块**，否则上一帧写过、本帧没写的批会留下
        /// 残留行（capacity 非零）被本帧仍有效的标签引用 ⇒ 凭空写行。只清本块，不碰另一 pass。
        void ResetPass(uint32_t pass_index);

        /// 写一条批分区（CPU 暂存）。批号越界 fail-fast；`row_base` 违对齐只报一次。
        void SetLayout(uint32_t pass_index, uint32_t batch_index,
                       uint32_t row_base, uint32_t capacity,
                       uint32_t topology, uint32_t group_size);

        /// 落批数。`batch_count` 超上限 ⇒ 返回 false 并把本 pass 批数清零（调用方不得接管）。
        bool Commit(uint32_t pass_index, uint32_t batch_count);

        /// 放弃本次装配：本 pass 批数归零（越界 pass 静默忽略）
        void Abort(uint32_t pass_index)
        {
            if (pass_index < kPasses)
                counts[pass_index] = 0;
        }

        uint32_t BatchCount(uint32_t pass_index) const { return counts[pass_index]; }

        /// 只读行视图（越界 ⇒ nullptr；调用方不得假定容量）
        const Row *RowAt(uint32_t pass_index, uint32_t batch_index) const
        {
            if (pass_index >= kPasses || batch_index >= counts[pass_index] || batch_index >= kMaxBatches)
                return nullptr;

            return &rows[BlockBase(pass_index) + batch_index];
        }

        /// 整张（两块）暂存表：落设备时整块一起落 —— 未写的那块保持零 ⇒ shader 据
        /// `capacity == 0` 跳过该 pass。
        const Row *Data() const { return rows; }
        static constexpr size_t DataBytes() { return size_t(kPasses * kBlockStride) * sizeof(Row); }
    };
}