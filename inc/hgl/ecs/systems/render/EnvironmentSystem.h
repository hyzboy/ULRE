#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class EnvironmentManager;
    }

    namespace ecs
    {
        /**
         * EnvironmentSystem
         *
         * 世界环境（**天空 / IBL**）编辑入口（瘦转发层）。
         * 环境数据与 GPU UBO 统一归 EnvironmentManager（GraphicsContext 模块）所有；
         * 本系统只按所属 world 的 Profile 转发编辑请求，不拥有任何 GPU 资源。
         *
         * ⚠ 主方向光的**级联阴影托管**已于 2026-10-07（T6a）拆出到 `MainLightShadowSystem`；
         *    光源参数（`SkyInfo.sun_*`）当前仍住在环境域，迁往**世界私有光源行**是下一步（T6b）——
         *    见 `doc/future/lighting-shadow-domain-split-plan.md` §3/§5。
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;

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
        };
    }//namespace ecs
}//namespace hgl
