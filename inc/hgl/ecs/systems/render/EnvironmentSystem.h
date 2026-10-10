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
         * 拆域结果（T6a + T6b）：级联阴影托管在 `MainLightShadowSystem`；**光源参数**
         *   （方向 / 颜色 / 强度 / 角径 / 阴影开关）在世界私有**光源行**（`LightSystem` /
         *   `WorldAddresses.addr_lights`）—— 本系统只剩**一个 provider 槽参数块**：
         *   `ambient_env`（环境光 / IBL 句柄）。
         *   ⚠ **2026-10-07：`sky` 段（原 `SkyInfo`）整删**（两个字段全仓无写者，取值搬成
         *   `sky_atmosphere.glsl` 内常量；`addr_sky` 同步退出世界表）⇒ 天空外观的编辑入口不复存在。
         *   见 `doc/future/lighting-shadow-domain-split-plan.md` §3。
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

            // 环境光 provider 槽（`ambient_light_provider`）的参数块。
            // 修改后需调用 MarkAmbientEnvDirty()。
            graph::AmbientEnvInfo *EditAmbientEnvInfo();
            // 2026-10-10（审计 §1.1）：`GetAmbientEnvInfo()` / `SetAmbientEnvInfo()` 全仓零调用者 ⇒ 删除
            //（读写都走 `EditAmbientEnvInfo()` + `MarkAmbientEnvDirty()`）。
            void MarkAmbientEnvDirty();
        };
    }//namespace ecs
}//namespace hgl
