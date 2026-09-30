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
    class PrimitiveComponent;

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
    class RenderPrimitiveCollectSystem : public System
    {
    private:

        ECSContext* world = nullptr;

        // Bumped whenever a frame rebuilds the global materialization tables
        // (i.e. a dirty frame that materializes at least one runtime-rows
        // primitive). Primitives whose last_materialize_epoch differs must be
        // re-materialized — their rows were rebuilt while they were skipped.
        uint64_t materialize_epoch = 0;

        bool ResolveMaterialProgramForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                MaterialRuntimeSlot &slot);
        bool ResolveForwardProgram(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                   MaterialRuntimeSlot &slot);
        bool ResolveShadowCasterProgram(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                        MaterialRuntimeSlot &slot);
        bool ResolveRuntimePipelineForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                MaterialRuntimeSlot &slot);

        // D9：阴影 pass 跳过/失败路径的统一收敛入口。
        // 推进该实例 slot 的 shadow_retry_frames（A4 起归**每实例侧**；A3 曾记在共享变体
        // 记录上）：首次跳过与跨越收敛上限各告警一次（含 primitive 名与原因）；返回本帧
        // 是否应 bump 静态级联 revision（前 kShadowRetryFullBumpFrames 帧每帧一次以快速
        // 收敛，之后每 kShadowRetryBumpPeriod 帧一次——限速自愈，避免持续失败时每帧全量
        // 重画）。
        bool AdvanceShadowRetry(MaterialRuntimeSlot &slot,
                                const char *reason,
                                const std::shared_ptr<PrimitiveComponent> &primitive_comp);
        bool MaterializeRecipeRowsForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                               MaterialRuntimeSlot &slot);

    public:

        RenderPrimitiveCollectSystem(const std::string& name = "RenderPrimitiveCollectSystem");
        ~RenderPrimitiveCollectSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
