#pragma once

/**
 * ModelAssemblyStage.h —— `ModelRefComponent` 三段渐进装配的**阶段驱动**。
 *
 * 背景与边界：
 *   · 组件本身只存请求（uri/句柄/策略）与进度位，**不**执行装配；
 *   · 装配规则（骨架 ⇒ 几何 ⇒ 材质，每帧至多推进一段、策略可否决请求、
 *     句柄失败即终态）此前内嵌在 `ModelStreamingSystem::InstantiateModelRefs` 里，
 *     与流送车道的内部结构（`AssetStream`）耦合；
 *   · 本驱动把「对单个组件推进一段」抽成纯状态机，宿主只负责**遍历组件**并注入
 *     「材质行绑定从哪来」。流送车道因此可以独立演进，装配语义不被牵动。
 *
 * 语义契约（改动前请先读）：
 *   1. 每帧**至多**推进一段（与历史示例行为一致，避免一帧内跨段跳过回调）；
 *   2. 纹理允许占位并异步流送：材质段不等待纹理就绪；
 *   3. 任一段失败 ⇒ 立刻终态（`failed_reported`），不回退、不重试；
 *   4. 每段回调至多一次（`*_reported` 位）。
 */

#include <hgl/ecs/components/ModelRefComponent.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class ModelAsset;
        class ModelAssetRegistry;
    }

    namespace ecs
    {
        class ECSContext;

        namespace assembly
        {
            /// 组件当前所处的装配段（只读诊断口径）
            enum class ModelAssemblyStage : uint8_t
            {
                Settled = 0,
                Request,
                Skeleton,
                Geometry,
                Material,
            };

            const char *GetModelAssemblyStageName(ModelAssemblyStage stage);

            ModelAssemblyStage GetModelAssemblyStage(const ModelRefComponent &ref);

            /// 失败落定：先置终态标志再回调，防止回调侧重入本段时重复触发。
            void FailModelAssembly(ModelRefComponent &ref, const char *reason);

            /**
             * @brief 材质行绑定的来源（由宿主注入）。
             *
             * 驱动本身不认识流送车道（`AssetStream`）的内部结构：调用方自己决定
             * 「哪个 asset 的行绑定在哪」，驱动只在组件还没有显式绑定行时问一次。
             * @return true = 已填入 out_rows/out_row_count
             */
            struct MaterialRowsProvider
            {
                void *user = nullptr;
                bool (*resolve)(void *user,
                                graph::ModelAsset *asset,
                                const graph::GlobalSSBOBinding *&out_rows,
                                uint32_t &out_row_count) = nullptr;
            };

            /**
             * @brief 对单个组件推进**至多一段**。
             * @param registry  默认句柄表（用于识别已被 `Clear()` 回收的句柄）
             * @param provider  材质行来源（可空 ⇒ 只按组件已有设定装配）
             * @return true 表示本次有阶段落定
             */
            bool AdvanceModelAssembly(ECSContext &context,
                                      ModelRefComponent &ref,
                                      graph::ModelAssetRegistry *registry,
                                      const MaterialRowsProvider &provider);
        }//namespace assembly
    }//namespace ecs
}//namespace hgl
