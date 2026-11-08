#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <unordered_map>

#include<hgl/math/geometry/Frustum.h>
#include<hgl/ecs/core/render/MaterialBatch.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKObjectNameBuilder.h>

namespace hgl
{
    namespace graph
    {
        struct CameraInfo;
        class VulkanDevice;
        class BufferManager;
        class GlobalSSBOBufferRegistry;
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
        // 2026-10-10（审计 §1.1）：`prepared_frame_index` 已删除 —— 它只被"读一次再写回"、
        // 没有任何 early return 用它 ⇒ 不是守卫（TextRenderPipeline 里同名字段是**真守卫**，勿混）。

    public:
        bool PrepareFrame(ECSContext* ctx);
        uint32_t GetPrepareFrameCount() const { return prepare_frame_count_; }   ///< §2 #11 判据用

        void RunCulling();
        void RunBatching();


    private:
        void PerformFrustumCulling();

        bool TestFrustumWithWorldAABB(RenderItem* item, const math::AABB& world_aabb);
        bool TestFrustumWithLocalAABB(RenderItem* item, const glm::vec3& local_center, const glm::vec3& local_extents);
        bool TestFrustumWithBoundingSphere(RenderItem* item);

        bool PrepareFrameInternal(ECSContext* ctx);   ///< §2 #11：真正的帧前准备（守卫只调它一次/帧）
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

        /// 本帧是否有任一含几何批正在接管（`TryApplyGPUCullOverride` 记账）。        /// T3i 起接管是**按批粒度**的 ⇒ 此判据只表示"至少一批走 GPU 账"。
        bool IsGPUCullOverrideActive() const { return gpu_cull_override_frame_ == gpu_cull_override_eval_frame_; }

        /// 本帧是否**全量**接管（全部含几何批都走 GPU 账）——只有全量才允许跳过
        /// CPU 剔除：部分接管时 CPU-driven 批仍需 CPU 剔除，否则它们会退化成
        /// "全项提交"（CPU 剔除是它们的唯一剔除来源）。
        bool IsGPUCullFullTakeoverActive() const { return gpu_cull_full_frame_ == gpu_cull_override_eval_frame_; }

        /// guard 帧号记账（0 = 从未评估 ⇒ 恒"不接管"）：见 `IsGPUCullOverrideActive`
        uint64_t gpu_cull_override_eval_frame_ = 0;
        uint64_t gpu_cull_override_frame_ = 0;
        uint64_t gpu_cull_full_frame_ = 0;

        /// P4：**模板部件事实缓存**（`ModelTemplatePool` / `ModelPartPool` 行内容的 CPU 侧镜像）。
        ///
        /// 为什么需要：guard 的模板展开归组循环逐项读 `ModelTemplateRow` 的
        /// `part_offset` / `part_count` / `part_bucket_count` —— 这些行落在**映射的 GPU 缓冲**
        /// （host-visible、非缓存内存）上，单字段读约 2.3µs；1000 项棋盘实测该循环占
        /// guard 的 58%（≈1.65ms）。模板数 ≪ 实例数 ⇒ 按 `template_id` 记忆化即可，
        /// 每帧成本从「项数 × 3 次映射读」降为「模板数 × 一次建表 + 项数 × 哈希查」。
        ///
        /// 失效：`(拓扑总代数, 池内容变更计数)` 二元组变化 ⇒ 整表清空。两者合起来覆盖
        /// 「模板行被改写」「部件行被改写」「池行被回收复用」的全部形态；重建只涉及本帧
        /// 实际出现的模板（典型 1~个位数）。
        struct TemplatePartFacts
        {
            uint32_t part_offset       = 0;
            uint32_t part_count        = 0;
            uint32_t part_bucket_count = 0;
            std::vector<uint32_t> slot_count;      ///< 槽 → 该槽部件数
            std::vector<uint32_t> slot_geometry;   ///< 槽 → geometry_id（该槽桶的几何体）
        };

        /// `ResolveTemplatePartFacts` 的失败形态（用于保持与原实现**逐字相同**的拒批原因）
        enum class TemplateFactResult : uint8_t
        {
            Ok = 0,
            RowMissing,    ///< 模板行缺失 / 无部件桶
            PartMissing,   ///< 模板 part 区间越界（部件行缺失）
        };

        bool     template_fact_valid_            = false;
        uint64_t template_fact_topology_rev_     = 0;
        uint64_t template_fact_pool_content_rev_ = 0;
        uint64_t template_fact_hits_             = 0;
        uint64_t template_fact_misses_           = 0;
        uint64_t template_fact_builds_           = 0;

        std::unordered_map<uint32_t, TemplatePartFacts> template_facts_;

        void RefreshTemplateFactCache();
        const TemplatePartFacts *ResolveTemplatePartFacts(graph::GlobalSSBOBufferRegistry *pool,
                                                          uint32_t template_id,
                                                          TemplateFactResult &result);

    public:
        /// P4：模板事实缓存统计（命中 = 免读映射行；未命中 = 本帧首次遇到该模板）
        uint64_t GetTemplateFactHits()     const { return template_fact_hits_; }
        uint64_t GetTemplateFactMisses()   const { return template_fact_misses_; }
        uint64_t GetTemplateFactBuilds()   const { return template_fact_builds_; }

    private:

        /// P4：**材质批计划缓存**（稳态重放）。
        ///
        /// 为什么需要：`BuildMaterialBatches()` 逐项解析「程序 → 管线 → 动态渲染状态 →
        /// 材质行」，其中 `GetPipeline` / `GetPipelineConfig` 各是一次
        /// `MaterialRuntimePipelineTable::entries.find(entity)`（`std::unordered_map`，
        /// 每实例一个散列节点）——1000 项实测 2.9ms，占 BatchPhase 的 95%。
        /// 这些解析结果在**拓扑不变**的帧里逐位相同，重复解析纯属浪费。
        ///
        /// 重放契约（命中时）：
        ///   · 只按缓存里的**键**重建 `MaterialBatch` 对象（`BeginFrame` 每帧清空批表，
        ///     批对象必须重建；但键与「项→批」归属不必重算）；
        ///   · 项指针按缓存里记录的「项 → 批下标」逐个 `AddItem`；
        ///   · 命中条件 = 总拓扑代数不变 + 材质内容事实代数不变 + **同一 render pass** +
        ///     **项序列指纹**（实体 index/generation/可见性，按序 FNV1a）一致 +
        ///     可见项数一致。
        ///   · 任一不满足 ⇒ 走原逐项路径并在末尾**整体重存**计划（拒绝半成品计划）。
        struct CachedBatchPlan
        {
            ShaderProgramPipelineKey key;
            uint32_t item_count = 0;   ///< 诊断：该批在计划里的项数
        };

        /// 每个 render pass 一份计划。
        /// Main / Shadow 用的是**不同**的 `RenderPass*`，共用一份会让两个 pass
        /// 每帧互相覆盖 ⇒ 永不命中。故按 pass 指针分槽。
        struct BatchPlanSlot
        {
            const void *render_pass  = nullptr;
            uint64_t    topology_rev = 0;
            uint64_t    fact_rev     = 0;
            uint64_t    item_sig     = 0;
            uint32_t    visible      = 0;
            bool        valid        = false;

            std::vector<CachedBatchPlan> batches;      ///< 计划批表（键，出现序）
            std::vector<uint32_t>        item_batch;   ///< 可见项序号 → 计划批下标
        };

        std::vector<BatchPlanSlot> batch_plans_;
        uint64_t batch_plan_hits_   = 0;
        uint64_t batch_plan_misses_ = 0;
        uint64_t batch_plan_builds_ = 0;

        BatchPlanSlot *AcquireBatchPlanSlot(const void *render_pass);
        BatchPlanSlot *FindBatchPlanSlot(const void *render_pass);
        void ResetMaterialBatchPlan();
        bool TryReplayMaterialBatchPlan(struct RenderFrameCache &cache,
                                        BatchPlanSlot &slot,
                                        TransformAssignmentBuffer *shared_transform_buffer,
                                        graph::BufferManager *buffer_manager);
        uint64_t ComputeRenderItemSignature(const struct RenderFrameCache &cache,
                                           uint32_t &visible_count) const;

    public:
        /// P4：材质批计划重放统计
        uint64_t GetBatchPlanHits()      const { return batch_plan_hits_; }
        uint64_t GetBatchPlanMisses()    const { return batch_plan_misses_; }
        uint64_t GetBatchPlanBuilds()    const { return batch_plan_builds_; }

    private:

        /// 批 1.3 C5：两个一次性闩改**每实例**（原 `static bool` 是进程级 ⇒ 第二个世界静默）：
        /// 前者 = 批前提不满足的 fail-fast 告警；后者 = 「GPU 剔除接管生效」的一次性正面证据日志。
        bool gpu_cull_reject_warned_  = false;
        bool gpu_cull_engaged_logged_ = false;

        /// §2 #11（2026-10-11）：`PrepareFrame` 的**真守卫**状态 —— 帧号 + 该帧结果缓存。
        /// 三个 `Run*`（Cull/Sort/Build）各自调用 `PrepareFrame`，原实现每帧最多做三遍同样的
        /// 帧前准备（Sort 那遍还没有下游动作）。注意与 2026-10-10 删掉的 `prepared_frame_index`
        /// 区分：那个只有"读一次再写回"、没有任何 early return，属**假守卫**；这里有 early return。
        uint64_t prepared_frame_      = UINT64_MAX;   ///< 上次已处理的帧号（UINT64_MAX = 从未）
        bool     prepared_frame_ok_   = false;        ///< 该帧的准备结果（同帧重复调用直接复用）
        uint32_t prepare_frame_count_ = 0;            ///< 真正做过准备的次数（判据用）

        graph::BufferManager* GetBufferManager() const;

        void FinalizeBatch(MaterialBatch& batch);
    };
}
