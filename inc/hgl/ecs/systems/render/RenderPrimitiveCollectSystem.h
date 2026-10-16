#pragma once

#include<hgl/ecs/core/System.h>
#include <hgl/ecs/support/MaterialRuntimeTable.h>

namespace hgl
{
    namespace graph
    {
        class CameraInfo;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class Entity;

    /**
     * RenderPrimitiveCollectSystem
     *
     * Collects primitive render items for the current frame.
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
    class RenderPrimitiveCollectSystem : public System
    {
    private:

        ECSContext* world = nullptr;

        // Bumped whenever a frame rebuilds the global materialization tables
        // (i.e. a dirty frame that materializes at least one runtime-rows
        // primitive). Primitives whose last_materialize_epoch differs must be
        // re-materialized — their rows were rebuilt while they were skipped.
        uint64_t materialize_epoch = 0;

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

        /// 批 1.3 C5：`ULRE_ARENA_DEBUG` 一次性 trace 闩改**每实例**（原 static 是进程级 ⇒ 第二个世界静默）
        bool arena_trace_done_ = false;

    public:

        RenderPrimitiveCollectSystem(const std::string& name = "RenderPrimitiveCollectSystem");
        ~RenderPrimitiveCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
