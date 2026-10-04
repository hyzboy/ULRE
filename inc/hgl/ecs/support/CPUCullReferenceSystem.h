#pragma once
#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/Context.h>
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

    private:
        ECSContext* context_ = nullptr;

        // 采样位图：下标 = EntityID.index（1 = 本采样窗口判可见）。
        // 容量 = 实体表容量（4096，`EntityGPUTable::kCapacity` 同口径）⇒ index
        // 恒在界内；比对 = 逐槽 O(kMaxVisible)。零动态分配、无顺序依赖
        //（替代旧 std::vector + std::set 台架，用户合规清单第 3 条）。
        static constexpr uint32_t kMaxVisible = 4096;

        uint8_t cpu_visible_map_[kMaxVisible] = {};
        uint8_t gpu_visible_map_[kMaxVisible] = {};

        uint32_t cpu_count_ = 0;
        uint32_t gpu_count_ = 0;

        // 对拍统计（最近一次采样窗口）
        ParityStats last_parity_stats_;

        // 采样节奏计数（Update 自降频：每 60 帧 = 一个采样窗口）
        uint32_t frame_counter_ = 0;
    };
}
