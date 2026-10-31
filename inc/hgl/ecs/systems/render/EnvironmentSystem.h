#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/type/String.h>
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

            // ── ambient provider **模块选择**（2026-10-14：从材质 TOML 上移到环境层）──
            //
            // 环境光有两条正交的轴，本系统是它们的**作者侧统一入口**：
            //   · 数据轴 = `EditAmbientEnvInfo()` + `MarkAmbientEnvDirty()`（IBL 句柄）；
            //   · 算法轴 = `SetAmbientProvider()`（用哪个 GLSL 模块，如 `indirect_ibl`）。
            // 材质只声明表面（`material_source_module_name` / `ntb_module_name`），
            // 不再声明 ambient ⇒ 同一套 PBR 材质在任何环境下都能用。
            //
            // `module_name` = GlslModuleRegistry 注册名（`@ulre name`）。
            // 空串 = 回到场景渲染模板默认 provider（`indirect_sky_ambient`）。
            // 返回 false = 环境管理不可用 / profile 无效（未做任何修改）。
            // ⚠ 换模块**必须**同时确认 `ambient_env` 已给有效句柄（IBL 模块采样它）：
            //   选了 IBL 却没给句柄 ⇒ 采样 0 句柄 ⇒ 静默发黑。
            bool SetAmbientProvider(const AnsiString &module_name);
            /// 当前生效的 ambient provider 模块名（空 = 模板默认）。
            const AnsiString &GetAmbientProvider() const;
        };
    }//namespace ecs
}//namespace hgl
