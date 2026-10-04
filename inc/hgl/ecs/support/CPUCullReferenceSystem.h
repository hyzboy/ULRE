#pragma once
#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Vector.h>

namespace hgl::ecs
{
    /**
     * CPU 参考剔除系统（T3d-2；T3g 台架化）
     * 
     * 职责：
     * - 在 CPU 侧实现与 GPU 相同的视锥剔除逻辑
     * - **每 60 帧一个采样窗口**：跑一遍 CPU 剔除 + 同步读回 GPU 结果 + 位图对拍
     *   （旧版每帧全量跑；T3g 派发异步化后，读回需要 fence ⇒ 每帧跑 = 每帧 stall，
     *   与"生产路径零同步"矛盾 ⇒ 台架成本只在采样帧付出）
     * - 打印对拍统计（一致/不一致数量）
     * 
     * 设计：
     * - 执行阶段：RenderCollect（在 GPUCullSystem 之后）
     * - 使用相同的剔除算法：6 平面 AABB 测试；geometry_id 读实体 4-ID 表
     *   （与 GPU 逐字同口径，§8.2d）
     * - 对拍用**定长位图**（下标 = EntityID.index 的布尔数组，2×4096 B，零动态
     *   分配）：atomicAdd 的幸存序每帧不定，位图把"集合相等"归约为逐槽比对——
     *   旧 std::set 台架是给"顺序不定"买的单，位图直接消掉。
     */
    class CPUCullReferenceSystem : public System
    {
    public:
        CPUCullReferenceSystem();
        ~CPUCullReferenceSystem() override = default;

        void Update(float delta_time) override;

        // 初始化
        bool Initialize(ECSContext* context);

        // 最近一次采样窗口的 CPU 可见数
        uint32_t GetVisibleCount() const { return cpu_count_; }

        // 获取 GPU 剔除计数（最近一次采样窗口的读数）
        uint32_t GetGPUVisibleCount() const;

        // 对拍统计
        struct ParityStats
        {
            uint32_t cpu_count = 0;
            uint32_t gpu_count = 0;
            uint32_t match_count = 0;
            uint32_t mismatch_count = 0;
        };
        ParityStats GetParityStats() const { return last_parity_stats_; }

    private:
        // CPU 侧视锥剔除（产出 cpu 位图 + 计数）
        void PerformCPUCulling();

        // 同步并读取 GPU 可见索引（产出 gpu 位图 + 计数）
        void ReadGPUVisibleIndices();

        // 位图对拍 + 打印统计
        void CompareResults();

        // T3h 值链判据（形态 B 接管中才有意义）：CPU 访问器 → EntityGPU 行事实位 →
        // gather mtl 行，逐字节三段一致。像素判据对 receive 位不敏感（场景无可见
        // 投影时全等是空转），值链判据才证明"接线存在且同式"。只查幸存集内的实体
        //（未进批的实体行 = 从未上行 ⇒ 0/0 属合法初值，不算 mismatch）。
        void CheckShadowFacts();

    private:
        ECSContext* context_ = nullptr;

        // 采样位图：下标 = EntityID.index（1 = 本采样窗口判可见）。
        // 容量 = 实体表容量（4096，`EntityGPUTable::kCapacity` 同口径）⇒ index
        // 恒在界内；比对 = 逐槽 O(kMaxVisible)。零动态分配、无顺序依赖
        //（替代旧 std::vector + std::set 台架，用户合规清单第 3 条）。
        static constexpr uint32_t kMaxVisible = 4096;

        uint8_t cpu_visible_map_[kMaxVisible] = {};
        uint8_t gpu_visible_map_[kMaxVisible] = {};

        // GPU 幸存**顺序表**（= visible_indices 原文：slot k ↔ gather 行表第 k 行）⇒
        // 值链判据用它把「实体 index → 其 gather mtl 行」对齐（位图丢序，这里保序）。
        uint32_t gpu_visible_order_[kMaxVisible] = {};

        // T3i（③ 多批）：值链走查需要「竞技场行 → 实体」的**精确**映射——多批
        // atomicAdd 交错下"第 k 行 = 第 k 个幸存者"不再成立（保序表在多批下作废）。
        // 映射走 **L2W 行号反查**：L2W 行空间 = 实体数 × 帧槽（每个 (实体, 帧槽)
        // 一行），上限 = kMaxVisible × HGL_FRAME_SLOT_TOTAL。
        // ⚠ 行号可能来自**未挂载 Transform 的实体**（row[1] 为哨兵值）⇒ 填表/查表
        // 都必须先做界内判定（曾经的越界写 = 崩溃根因）。
        static constexpr uint32_t kL2WRowCapacity = kMaxVisible * HGL_FRAME_SLOT_TOTAL;

        uint32_t l2w_row_to_entity_[kL2WRowCapacity] = {};
        const Entity* alive_owner_[kMaxVisible] = {};   ///< index → 实体指针（采样窗口重建）

        uint32_t cpu_count_ = 0;
        uint32_t gpu_count_ = 0;

        // 对拍统计（最近一次采样窗口）
        ParityStats last_parity_stats_;

        // 采样节奏计数（Update 自降频：每 60 帧 = 一个采样窗口）
        uint32_t frame_counter_ = 0;
    };
}
