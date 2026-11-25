#pragma once

#include<hgl/ecs/core/System.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterializationEpochState.h>
#include <hgl/ecs/support/storage/CollectCacheState.h>
#include <hgl/type/ValueArray.h>

namespace hgl
{
    namespace graph
    {
        struct CameraInfo;
    }
}

namespace hgl::ecs
{
    /// 主帧拓扑快照的成员记录（P2）。此处只需前置声明：仅作**可选入参指针**使用。
    struct SnapshotItemRecord;

    class ECSContext;
    class Entity;
    class PrimitiveRenderItem;

    /**
     * RenderCollectSystem
     *
     * Normalizes geometry entities to model templates and collects one item per Shading Group.
     *
     * A4：材质运行期状态有两处载体（再无"每实体一个材质运行期组件"）：
     *   · **共享行**（`MaterialRuntimeTable::MaterialRuntimeRow`，世界私有、interned）：
     *     可跨实体共享的绑定（变体 ID / 材质 SSBO 行与地址 / 纹理引用行与 hash /
     *     已解析配方缓存）；
     *   · **每实例 slot**（`MaterialRuntimeSlot`，世界表按实体稀疏存放）：
     *     每帧/每实例可变状态（pass/LOD/dither 选择器、D9 重试与降频计数、脏标志、
     *     每实体授权代跟踪副本）。
     */
    // §2 #3（2026-10-11）：本文件实现里有 **9 处 `world->IsCurrentPassShadow()`**
    // （`:800,:1027,:1318,:2118,:2152,:2372,:2391,:2421,:2425`），与头注所言
    // 「渲染策略判定表（`RenderStrategyTable`）是图元收集与阴影能力的**唯一判据**」看似冲突。
    // 三方复核的分类（不是第二套判据，因此**不收敛**）：
    //   · **1 处 = 事实注入**：把"本帧是不是 shadow pass"填进 gathered facts，供策略表消费；
    //   · **6 处 = 程序选择/重试/状态机**：按 pass 选程序、D9 阴影重试推进与降频、本帧状态复位；
    //   · **2 处 = 诊断**：日志/计数里的 pass 归属。
    // 分工口径：**能力与意图**由策略表唯一裁决；上面 9 处只是"在本 pass 内怎么执行"的局部机制。
    // 若后续要收敛，必须先证明局部机制可由策略表驱动（会改动 pass 分流与 D9 重试链，风险 > 收益）。
    class RenderCollectSystem : public System
    {
    private:

        ECSContext* world = nullptr;

        /// 物化世代 + 本帧已刷新的模板纹理行 + ambient provider 快照（三者必须同步推进）
        MaterializationEpochState materialization;

        /// P1 干跑代数 + P3 稳态判据与命中统计（只由本系统内部推进）
        CollectCacheState collect_cache;

        /// P1：每帧一次的资格判定与统计（只观测，**不跳过任何逻辑**）。
        void ObserveCollectCacheEligibility();

        /// P3：主帧收集快路径。
        ///
        /// 返回 true = 本 pass 已完成全部收集工作（调用方**必须立即 return**）；
        /// false = 未接管（任何一条前提不成立，或过程中发现不一致而主动回退），
        /// 调用方照原样跑完整收集。回退时不会留下半成品：`cache` 里已建的项会被
        /// 清掉，已发布的事实行写的都是权威值（完整路径会再写一遍同样的值）。
        bool TryCachedMainFrameCollect(bool ambient_provider_changed);

        /// 发布一个已装配图元的**全部权威事实**。
        ///
        /// 事实清单（缺一不可，缺哪一项都是静默错画）：
        ///   · 4-ID 行（transform / geometry / material / texture，**按 pass 分半存**）；
        ///   · 阴影事实（`CanReceiveShadow` → 接收位 + bias 归一行）；
        ///   · 可见性（`Visible` 位，权威在 `VisibilityDataStorage`，含祖先继承）；
        ///   · 模型模板事实（`template_id` / 部件掩码 / custom_data_index / prev_transform_id）。
        ///
        /// **完整收集路径与 P3 快路径共用这一份实现**：任何一侧要增删一种事实，
        /// 都只改这里一次，不可能出现「快路径漏发一个事实」的静默偏差。
        ///
        /// @param entity_id    图元实体
        /// @param transform_id 变换行号（`TransformAccessor::GetID()`）
        /// @param geometry_id  几何行号；`0` = 无几何 ⇒ 只做本 pass 那一半的显式清零
        /// @param material_id  材质数据行号（0 = 无）
        /// @param texture_id   材质纹理配置行号（0 = 无）
        /// @param pass_index   0 = 主帧，1 = 阴影（4-ID 行按 pass 分半存）
        /// @param item         已装配的图元项（`draw_key` 由本函数填）
        /// @param draw_seed    **可选**：上一份主帧拓扑快照的记录。非空时绘制键直接取自它，
        ///                     从而**完全不读** `MeshDrawParams` 的 CPU 行（该行在映射的 GPU
        ///                     缓冲里，实测每字段 ≈2.3 µs）。快路径实测 16.4 → 1.7 ms。
        ///                     语义前提：记录来自同一拓扑代数 ⇒ `geometry_id` 未变
        ///                     ⇒ 该 `geometry_id` 对应的绘制键也未变（见 P2 内容哈希）。
        /// @return true = 几何行成立、4-ID 已发布；false = 本 pass 那一半已显式清零
        bool PublishPrimitiveFacts(EntityID entity_id,
                                   uint32_t transform_id,
                                   uint32_t geometry_id,
                                   uint32_t material_id,
                                   uint32_t texture_id,
                                   uint32_t pass_index,
                                   PrimitiveRenderItem *item,
                                   const SnapshotItemRecord *draw_seed = nullptr);

        /// 命名口径：**Ensure 而非 Resolve** —— "Resolve" 会被读成"只查表"，
        /// 而这三个入口在缓存未命中时**会触发** GLSL 生成 + SPV 编译 + 磁盘落盘
        /// （经 ShaderProgramManager::AcquireShaderProgram）。
        bool EnsureMaterialProgramForPrimitive(Entity *primitive_comp,
                                               MaterialRuntimeSlot &slot);
        bool EnsureForwardProgram(Entity *primitive_comp,
                                  MaterialRuntimeSlot &slot);
        bool EnsureShadowCasterProgram(Entity *primitive_comp,
                                       MaterialRuntimeSlot &slot);
        bool ResolveRuntimePipelineForPrimitive(Entity *primitive_comp,
                                                MaterialRuntimeSlot &slot);

        // D9：阴影 pass 跳过/失败路径的统一收敛入口。
        // 推进该实例 slot 的 shadow_retry_frames（A4 起归**每实例侧**；A3 曾记在共享变体
        // 记录上）：首次跳过与跨越收敛上限各告警一次（含 primitive 名与原因）；返回本帧
        // 是否应 bump 静态级联 revision（前 kShadowRetryFullBumpFrames 帧每帧一次以快速
        // 收敛，之后每 kShadowRetryBumpPeriod 帧一次——限速自愈，避免持续失败时每帧全量
        // 重画）。
        bool AdvanceShadowRetry(MaterialRuntimeSlot &slot,
                                const char *reason,
                                Entity *primitive_comp);
        bool MaterializeRecipeRowsForPrimitive(Entity *primitive_comp,
                                               MaterialRuntimeSlot &slot);

        /// 阶段 1：前置确保图元实体的 GPUModelRefComponent 与模型模板
        bool EnsurePrimitiveModelTemplate(Entity *entity);

    public:

        RenderCollectSystem(const std::string& name = "RenderCollectSystem");
        ~RenderCollectSystem() override = default;

    public:

        // ── P3 收集快路径统计（报表用；快路径关闭时恒为 0）───────────────────

        /// 快路径完整接管的主帧数
        uint64_t GetCollectCacheHits()      const { return collect_cache.Hits(); }
        /// 尝试后被判据拒绝 / 中途发现不一致而回退的次数
        uint64_t GetCollectCacheFallbacks() const { return collect_cache.Fallbacks(); }
        /// 最近一次快路径重建出的成员数
        uint64_t GetCollectCacheLastItems() const { return collect_cache.LastItems(); }
        /// 上一帧主帧收集是否为稳态（快路径的前提之一）
        bool     IsLastMainCollectSteady()  const { return collect_cache.IsLastMainCollectSteady(); }

        void SetWorld(ECSContext* w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
