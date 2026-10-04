#pragma once

#include <cstdint>
#include <limits>
#include <utility>

#include<hgl/math/geometry/Frustum.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/support/DrawItemCompaction.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKObjectNameBuilder.h>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
        class VulkanDevice;
        class BufferManager;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class TransformSystem;
    class RenderItem;
    class PrimitiveRenderItem;

    class PrimitiveBatchPipeline
    {
        OBJECT_LOGGER

    private:
        ECSContext* world = nullptr;
        const graph::CameraInfo* camera_info = nullptr;
        graph::VulkanDevice* device = nullptr;
        math::Frustum frustum;
        uint32_t prepared_frame_index = std::numeric_limits<uint32_t>::max();
        CompactionStats last_compaction_stats{};

    public:
        bool PrepareFrame(ECSContext* ctx);

        void RunCulling();
        void RunSorting();
        void RunTransformIndexing();
        void RunBatching();

        const CompactionStats &GetLastCompactionStats() const { return last_compaction_stats; }

    private:
        void PerformFrustumCulling();

        bool TestFrustumWithWorldAABB(RenderItem* item, const math::AABB& world_aabb);
        bool TestFrustumWithLocalAABB(RenderItem* item, const glm::vec3& local_center, const glm::vec3& local_extents);
        bool TestFrustumWithBoundingSphere(RenderItem* item);

        void SortByDistance();
        void AssignTransformIndices(TransformSystem* transform_system);
        void BuildMaterialBatches();
        void FinalizeBatches();

        /// T3f-2（形态 B）：GPU 剔除 gather 输出的**接管 guard**（`FinalizeBatches()`
        /// 开头、任何 `FinalizeBatch` 之前调用；逐帧评估）。全部前提通过 ⇒ 通过
        /// `GPUCullSystem::ApplyGPUCullOverrideHook()` 把该批的行表/icb/count 指向
        /// 剔除输出并置 `gpu_driven_override`（既有跳过契约）。
        /// 任一前提不满足 ⇒ 本帧不接管（批留在 CPU 路径），首次失败报一次错。
        /// 前提清单（§8.3；口径 = "gather 写出的值与 CPU 行表逐字节等价"）：
        ///   opt-in 开 && 主帧 pass（shadow 批共用 materialBatches 表，实测本场景每帧
        ///   仅 1 批，判据仍要显式挡 pass）&& **恰好一个含几何批**（R4 单批口径：
        ///   纹理基址/材质池按批单值 push）&& 全 mesh 批 && 非 lines &&
        ///   全 PrimitiveRenderItem（禁实例化/多实例——gather 每幸存实体一命令 Y=1）&&
        ///   批容量 ≤ gather 容量 && 全 item 有 owner 实体（4-ID 表只覆盖实体）&&
        ///   每实体 4-ID 行在 && 池行在 && 非 meshlet（池行 addr_meshlets==0 ⇒ X 组数
        ///   公式与 CPU `GetMeshletCount()` 分支不同源，挡在接管外）&&
        ///   阴影全默认（`CanReceiveShadow && bias==1.0` ⇒ gather 写 0/1.0 语义与 CPU 一致）。
        void TryApplyGPUCullOverride();

        /// 本帧是否正在接管（`TryApplyGPUCullOverride` 记账；collect 的 CPU 剔除跳不跳
        /// 读它 ⇒ 两本账恒二选一，杜绝双份账/漏帧）
        bool IsGPUCullOverrideActive() const { return gpu_cull_override_frame_ == gpu_cull_override_eval_frame_; }

        /// guard 帧号记账（0 = 从未评估 ⇒ 恒"不接管"）：见 `IsGPUCullOverrideActive`
        uint64_t gpu_cull_override_eval_frame_ = 0;
        uint64_t gpu_cull_override_frame_ = 0;

        graph::BufferManager* GetBufferManager() const;
        graph::ObjectNameBuilder BuildICBNames() const;

        void ReallocICB(MaterialBatch& batch);
        void BuildBatches(MaterialBatch& batch, const uint32_t base_instance);
        void EnsureMeshDrawParams(MaterialBatch& batch);       ///<mesh per-draw 参数表容量保障
        void WriteMeshDrawCommands(MaterialBatch& batch);      ///<mesh 命令 + 参数行（与 draw_batches 同序）
        void FinalizeBatch(MaterialBatch& batch);
        void SortBatchItems(MaterialBatch& batch);
        void EnsureBatchIndexRows(MaterialBatch& batch);
        void WriteBatchIndexRows(MaterialBatch& batch);
    };
}
