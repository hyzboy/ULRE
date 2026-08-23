#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/SkyInfo.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>

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
         * 世界环境编辑入口（瘦转发层）。
         * 环境数据与 GPU UBO 统一归 EnvironmentManager（GraphicsContext 模块）所有；
         * 本系统只按所属 world 的 RT 选中的 Profile 转发编辑请求，不拥有任何 GPU 资源。
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;

            graph::EnvProfileID ResolveProfileID() const;
            graph::EnvironmentManager *ResolveManager();

        public:

            EnvironmentSystem(const std::string &name = "EnvironmentSystem");
            ~EnvironmentSystem() override = default;

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
