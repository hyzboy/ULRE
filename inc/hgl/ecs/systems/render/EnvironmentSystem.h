#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/SkyInfo.h>
#include<hgl/graph/ubo/ShadowInfo.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/graph/render/RenderTargetDesc.h>
#include<hgl/graph/module/RenderTargetManager.h>

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
         * EnvironmentSystem
         *
         * 世界环境编辑入口（瘦转发层）。
         * 环境数据与 GPU UBO 统一归 EnvironmentManager（GraphicsContext 模块）所有；
         * 本系统只按所属 world 的 RT 选中的 Profile 转发编辑请求，不拥有任何 GPU 资源。
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;

            // ── 主光级联阴影托管 ──
            graph::CascadedShadowController *shadow_controller = nullptr;
            graph::RenderTargetHandle cascade_rts[graph::kMaxShadowCascades]{};
            std::shared_ptr<CameraComponent> light_camera;
            bool shadow_enabled = false;
            uint32_t cascade_handles[graph::kMaxShadowCascades] = {};
            float cascade_depth_range[graph::kMaxShadowCascades] = {};

            graph::EnvProfileID ResolveProfileID() const;
            graph::EnvironmentManager *ResolveManager();

        public:

            EnvironmentSystem(const std::string &name = "EnvironmentSystem");
            ~EnvironmentSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }

            // 编辑本 world 当前使用的 Profile（RT 未设置即 default）。
            // 修改后需调用 MarkSkyDirty()。
            graph::SkyInfo *EditSkyInfo();
            const graph::SkyInfo *GetSkyInfo() const;

            void SetSkyInfo(const graph::SkyInfo &info, bool immediate = true);
            void MarkSkyDirty();

            // 编辑 ShadowInfo
            graph::ShadowInfo *EditShadowInfo();
            const graph::ShadowInfo *GetShadowInfo() const;

            void SetShadowInfo(const graph::ShadowInfo &info, bool immediate = true);
            void MarkShadowDirty();

            // ── 主光源级联阴影自动化托管 ──
            bool EnableMainLightShadow(const graph::CascadedShadowConfig &config, uint32_t shadow_map_size = 1024);
            void DisableMainLightShadow();
            bool IsMainLightShadowEnabled() const { return shadow_enabled; }

            graph::CascadedShadowController *GetShadowController() const { return shadow_controller; }
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
