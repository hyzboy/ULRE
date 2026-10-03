#pragma once
#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Vector.h>
#include <vector>

namespace hgl::ecs
{
    /**
     * CPU 参考剔除系统（T3d-2）
     * 
     * 职责：
     * - 在 CPU 侧实现与 GPU 相同的视锥剔除逻辑
     * - 每帧对比 GPU 和 CPU 的剔除结果
     * - 打印对拍统计（一致/不一致数量）
     * 
     * 设计：
     * - 执行阶段：RenderCollect（在 GPUCullSystem 之后）
     * - 使用相同的剔除算法：6 平面 AABB 测试
     * - 读取相同的输入：EntityGPU 表、L2W 矩阵、GeometryAABB、相机信息
     */
    class CPUCullReferenceSystem : public System
    {
    public:
        CPUCullReferenceSystem();
        ~CPUCullReferenceSystem() override = default;

        void Update(float delta_time) override;

        // 初始化
        bool Initialize(ECSContext* context);

        // 获取 CPU 剔除结果
        const std::vector<uint32_t>& GetVisibleIndices() const { return cpu_visible_indices_; }
        uint32_t GetVisibleCount() const { return static_cast<uint32_t>(cpu_visible_indices_.size()); }

        // 获取 GPU 剔除结果（从 GPUCullSystem 读取）
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
        // CPU 侧视锥剔除
        void PerformCPUCulling();

        // 从 GPU buffer 读取可见索引
        void ReadGPUVisibleIndices();

        // 对比 GPU 和 CPU 结果
        void CompareResults();

        // 视锥平面测试（与 GLSL 侧相同）
        bool TestFrustumPlane(const math::Matrix4f& l2w,
                             const math::Vector3f& aabb_center,
                             const math::Vector3f& aabb_extents,
                             const math::Vector4f& plane) const;

    private:
        ECSContext* context_ = nullptr;

        // CPU 剔除结果
        std::vector<uint32_t> cpu_visible_indices_;

        // GPU 剔除结果（从 buffer 读取）
        std::vector<uint32_t> gpu_visible_indices_;

        // 对拍统计
        ParityStats last_parity_stats_;

        // 容量
        static constexpr uint32_t kMaxVisibleEntities = 4096;
    };
}
