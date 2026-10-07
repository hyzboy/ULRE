#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/support/CameraSlotGuard.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/graph/render/RenderTargetDesc.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class EnvironmentManager;
        struct CascadedShadowConfig;
        class CascadedShadowController;
    }

    namespace ecs
    {
        class CameraComponent;

        /**
         * MainLightShadowSystem
         *
         * **主光源（主方向光）的级联阴影托管域**。
         *
         * 2026-10-07（T6a）自 `EnvironmentSystem` 原样搬出，行为零变化。归本域的：
         * CSM 控制器 / 4 张级联 RT / 光相机与其相机槽 RAII / 级联屏蔽 / 静态缓存失效钩子
         * / `ShadowInfo` 的物化与发布。
         *
         * ⚠ 光源的**定义**（方向/颜色/强度）当前仍住在 `SkyInfo`（环境域）：T6a 里本系统经
         *   `GetSystem<EnvironmentSystem>()` 读 sky；迁到**世界私有光源行**后此依赖消失
         *   （L2 步；见 `doc/future/lighting-shadow-domain-split-plan.md` §3/§5）。
         */
        class MainLightShadowSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;

            // controller 由本系统独占持有（unique_ptr）；类型在此仅前向声明，
            // 析构在 .cpp 中定义以支持不完整类型成员。
            std::unique_ptr<graph::CascadedShadowController> shadow_controller;
            graph::RenderTargetHandle cascade_rts[graph::kMaxShadowCascades]{};
            std::shared_ptr<CameraComponent> light_camera;

            /// 光相机占的**本世界**相机槽的 RAII 拥有者（C3）：Enable 申请、Disable/系统销毁归还
            CameraSlotGuard light_camera_slot;
            bool shadow_enabled = false;
            uint32_t cascade_mask = 0; // bit c == 1 表示屏蔽级联 c
            uint32_t cascade_handles[graph::kMaxShadowCascades] = {};
            float cascade_depth_range[graph::kMaxShadowCascades] = {};

            // A3：上次消费的 static_scene_revision——与 context 当前值不等即
            // 静态级联失效重建一次，随后追平不再触发。
            uint64_t consumed_static_scene_revision = 0;

            // 影子数据当前仍存于本世界**环境 profile** 的 ShadowInfo（`EnvironmentInfo::shadow`）；
            // 世界私有化属下一步（见设计档 P4）。
            graph::EnvProfileID ResolveProfileID() const;
            graph::EnvironmentManager *ResolveManager();

        public:

            MainLightShadowSystem(const std::string &name = "MainLightShadowSystem");
            ~MainLightShadowSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }

            // 编辑 ShadowInfo
            graph::ShadowInfo *EditShadowInfo();
            const graph::ShadowInfo *GetShadowInfo() const;

            void SetShadowInfo(const graph::ShadowInfo &info, bool immediate = true);
            void MarkShadowDirty();

            // ── 主光源级联阴影自动化托管 ──
            bool EnableMainLightShadow(const graph::CascadedShadowConfig &config, uint32_t shadow_map_size = 1024);
            void DisableMainLightShadow();
            bool IsMainLightShadowEnabled() const { return shadow_enabled; }

            // A3：静态级联滚动缓存（CSM 1..N）的失效钩子。
            // 静态物体的 L2W 变更已由 TransformSystem 检出并经
            // ECSContext::static_scene_revision 自动触发；本 API 供引擎外信号
            // 使用——运行时新增/删除静态物体、替换其材质/贴图等不经
            // TransformSystem 的变更，调用后下一帧静态级联全量重建。
            void InvalidateMainLightStaticShadowCache();

            // ── 调试与调优：级联屏蔽控制（支持运行时关闭/打开指定层） ──
            void SetCascadeMask(uint32_t mask) { cascade_mask = mask; }
            uint32_t GetCascadeMask() const { return cascade_mask; }
            void SetCascadeEnabled(uint32_t c, bool enabled)
            {
                if (c < graph::kMaxShadowCascades)
                {
                    if (enabled) cascade_mask &= ~(1u << c);
                    else cascade_mask |= (1u << c);
                }
            }
            bool IsCascadeEnabled(uint32_t c) const
            {
                return (c < graph::kMaxShadowCascades) && ((cascade_mask & (1u << c)) == 0);
            }

            graph::CascadedShadowController *GetShadowController() const { return shadow_controller.get(); }
            graph::IRenderTarget *GetCascadeRenderTarget(uint32_t cascade_index) const;
            float GetCascadeDepthRange(uint32_t cascade_index) const
            {
                return (cascade_index < graph::kMaxShadowCascades) ? cascade_depth_range[cascade_index] : 0.0f;
            }

            /// 驱动主光源阴影 Pass（全自动解算 4 级联、更新 UBO 并光栅化深度图）
            void RenderMainLightShadowPass(CameraComponent *main_camera, float deltaTime);
        };
    }//namespace ecs
}//namespace hgl
