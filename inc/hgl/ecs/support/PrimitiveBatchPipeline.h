#pragma once

#include <cstdint>
#include <limits>
#include <utility>

#include<hgl/math/geometry/Frustum.h>
#include<hgl/ecs/core/MaterialBatch.h>
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

    public:
        bool PrepareFrame(ECSContext* ctx);

        void RunCulling();
        void RunSorting();
        void RunTransformIndexing();
        void RunBatching();


    private:
        void PerformFrustumCulling();

        bool TestFrustumWithWorldAABB(RenderItem* item, const math::AABB& world_aabb);
        bool TestFrustumWithLocalAABB(RenderItem* item, const glm::vec3& local_center, const glm::vec3& local_extents);
        bool TestFrustumWithBoundingSphere(RenderItem* item);

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
        ///   全 PrimitiveRenderItem（gather 每幸存实体一命令 Y=1）&&
        ///   批容量 ≤ gather 容量 && 全 item 有 owner 实体（4-ID 表只覆盖实体）&&
        ///   T3i：**逐批独立评估**（按批粒度接管）——每批要求 mesh stage /
        ///   每实体 4-ID 行在 && 池行在；不满足的批只是**该批**不接管
        ///   （标签留无效 ⇒ cull 不写行、CPU 账照旧）。
        ///   A6：**lines 已可接管**（dispatch 档随批信息行下发：线按段数 + 组大小 64）。
        void TryApplyGPUCullOverride();

        /// 本帧是否有任一含几何批正在接管（`TryApplyGPUCullOverride` 记账）。
        /// T3i 起接管是**按批粒度**的 ⇒ 此判据只表示"至少一批走 GPU 账"。
        bool IsGPUCullOverrideActive() const { return gpu_cull_override_frame_ == gpu_cull_override_eval_frame_; }

        /// 本帧是否**全量**接管（全部含几何批都走 GPU 账）——只有全量才允许跳过
        /// CPU 剔除：部分接管时 CPU-driven 批仍需 CPU 剔除，否则它们会退化成
        /// "全项提交"（CPU 剔除是它们的唯一剔除来源）。
        bool IsGPUCullFullTakeoverActive() const { return gpu_cull_full_frame_ == gpu_cull_override_eval_frame_; }

        /// guard 帧号记账（0 = 从未评估 ⇒ 恒"不接管"）：见 `IsGPUCullOverrideActive`
        uint64_t gpu_cull_override_eval_frame_ = 0;
        uint64_t gpu_cull_override_frame_ = 0;
        uint64_t gpu_cull_full_frame_ = 0;

        graph::BufferManager* GetBufferManager() const;

        void EnsureMeshDrawParams(MaterialBatch& batch);       ///<mesh per-draw 参数表容量保障（gather 消费该池 ⇒ 必须物化）
        void FinalizeBatch(MaterialBatch& batch);
        void EnsureBatchIndexRows(MaterialBatch& batch);
        void WriteBatchIndexRows(MaterialBatch& batch);
    };
}
