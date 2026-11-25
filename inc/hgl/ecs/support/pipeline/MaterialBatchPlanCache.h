#pragma once

#include<hgl/ecs/core/render/ShaderProgramPipelineKey.h>
#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>
#include<hgl/type/PtrArray.h>
#include<hgl/type/ValueArray.h>
#include<cstdint>

namespace hgl
{
    namespace graph
    {
        class BufferManager;
        class VulkanDevice;
        struct CameraInfo;
    }
}

namespace hgl::ecs
{
    struct RenderFrameCache;
    struct MaterialBatch;
    class RenderItem;
    class TransformAssignmentBuffer;

    /**
     * 材质批计划缓存（批管线私有；P4 稳态重放）。
     *
     * 为什么需要：`BuildMaterialBatches()` 逐项解析「程序 → 管线 → 动态渲染状态 → 材质行」，
     * 其中 `GetPipeline` / `GetPipelineConfig` 各是一次世界侧运行期管线表查询
     * （按实体和 Shading Group，校验当前 pass）。这些解析结果在**拓扑不变**的帧里逐位相同。
     *
     * 重放契约（命中时）：
     *   · 只按缓存里的**键**重建 `MaterialBatch` 对象（`BeginFrame` 每帧清空批表，
     *     批对象必须重建；但键与「项→批」归属不必重算）；
     *   · 项指针按缓存里记录的「项 → 批下标」逐个 `AddItem`；
     *   · 命中条件 = 总拓扑代数不变 + 材质内容事实代数不变 + **同一 render pass** +
     *     **项序列指纹**（实体 index/generation/可见性，按序 FNV1a）一致 + 可见项数一致。
     *   · 任一不满足 ⇒ 走原逐项路径并在末尾**整体重存**计划（拒绝半成品计划）。
     */
    class MaterialBatchPlanCache
    {
    public:
        /// 材质批计划里「本帧跳过该项」的哨兵（该项解析不出管线/配方行 ⇒ 不进行）
        static constexpr uint32_t kNoPlanBatch = 0xFFFFFFFFu;

        struct CachedBatchPlan
        {
            ShaderProgramPipelineKey key;
            uint32_t item_count = 0;   ///< 诊断：该批在计划里的项数
        };

        /// 每个 render pass 一份计划。
        /// Main / Shadow 用的是**不同**的 `RenderPass*`，共用一份会让两个 pass
        /// 每帧互相覆盖 ⇒ 永不命中。故按 pass 指针分槽。
        struct Slot
        {
            const void *render_pass  = nullptr;
            uint64_t    topology_rev = 0;
            uint64_t    fact_rev     = 0;
            uint64_t    item_sig     = 0;
            uint32_t    visible      = 0;
            bool        valid        = false;

            ValueArray<CachedBatchPlan> batches;      ///< 计划批表（键，出现序）
            ValueArray<uint32_t>        item_batch;   ///< 可见项序号 → 计划批下标
        };

    private:
        /// PtrArray：槽指针交给调用方后在本次 BuildMaterialBatches 内保持稳定
        PtrArray<Slot> plans;

        uint64_t hits   = 0;
        uint64_t misses = 0;
        uint64_t builds = 0;

        Slot *Find(const void *render_pass);

    public:
        /// 全部计划作废（保留槽，只清内容）
        void Reset();

        /// 取（必要时新建）某 pass 的槽
        Slot *Acquire(const void *render_pass);

        /**
         * 项序列指纹：只读 `RenderItem` 的内联字段（实体 index/generation + 可见性），
         * **不触碰任何 SSBO 池**（那些行在映射的 GPU 缓冲里，单字段 ≈2.3µs）。
         *
         * 指纹变了 ⇒ 批计划作废。材质行/管线变化不需要进指纹 —— 由
         * `MaterialBinding`/`PassAndTarget` 拓扑代数与 `MaterialContent` 事实代数覆盖。
         */
        uint64_t ComputeSignature(const RenderFrameCache &cache, uint32_t &visible_count) const;

        /**
         * 命中路径：按计划重建 `MaterialBatch` 并逐项 `AddItem`。
         * 返回 true = 本帧批表已完整重建（调用方必须立即 return）；
         * false = 未命中（统计已累加），调用方照原样跑逐项路径。
         * 计划自洽性预校验失败时整份计划作废，绝不在已部分构建的批表上留半成品。
         */
        bool TryReplay(RenderFrameCache &cache,
                       Slot &slot,
                       const PrimitiveTopologyLedger &ledger,
                       TransformAssignmentBuffer *shared_transform_buffer,
                       graph::BufferManager *buffer_manager,
                       graph::VulkanDevice *device);

        /// 逐项路径跑完后整体重存（只有跑完全程才提交）
        void Store(Slot &slot,
                   uint64_t topology_rev,
                   uint64_t fact_rev,
                   uint64_t item_sig,
                   uint32_t visible,
                   ValueArray<CachedBatchPlan> &&batches,
                   ValueArray<uint32_t> &&item_batch);

        uint64_t Hits()   const { return hits; }
        uint64_t Misses() const { return misses; }
        uint64_t Builds() const { return builds; }
    };
}