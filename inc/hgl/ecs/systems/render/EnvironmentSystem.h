#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/SkyInfo.h>
#include<hgl/graph/ubo/ShadowInfo.h>
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
        // controller 由本系统独占持有（unique_ptr）；类型在此仅前向声明，
        // 析构在 EnvironmentSystem.cpp 中定义以支持不完整类型成员。
            std::unique_ptr<graph::CascadedShadowController> shadow_controller;
            graph::RenderTargetHandle cascade_rts[graph::kMaxShadowCascades]{};
            std::shared_ptr<CameraComponent> light_camera;
            bool shadow_enabled = false;
            uint32_t cascade_mask = 0; // bit c == 1 表示屏蔽级联 c
            uint32_t cascade_handles[graph::kMaxShadowCascades] = {};
            float cascade_depth_range[graph::kMaxShadowCascades] = {};

            // A3：上次消费的 static_scene_revision——与 context 当前值不等即
            // 静态级联失效重建一次，随后追平不再触发。
            uint64_t consumed_static_scene_revision = 0;

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
