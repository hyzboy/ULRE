#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/ubo/LightRow.h>
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>

namespace hgl
{
    namespace ecs
    {
        /**
         * LightSystem
         *
         * **光源域**：世界私有光源行表（`LightRowTable`，设备地址 `WorldAddresses.addr_lights`）的
         * **唯一编辑入口**。行 0 恒为**主方向光**（`graph::kMainLightRowIndex`）。
         *
         * 边界（`doc/future/lighting-shadow-domain-split-plan.md` §3）：
         *   · 拥有：光源行的编辑 API（方向 / 颜色 / 强度 / 角径 + 时刻模型落行）；
         *   · 不拥有：任何 GPU 资源、渲染、阴影机制。CSM 资源在 `MainLightShadowSystem`，它**读**行 0
         *     （单向：阴影读光，光源域不反向依赖阴影域）；本系统对它零依赖。
         *   · 行表本体由**世界**持有（`ECSContext::GetLightRowTable`）：世界只要渲染就必须有光源行，
         *     不能依赖"本系统被注册"——否则"世界有渲染、无光源"= 全黑且不报任何错。
         *
         * 写路径一律经 `LightRowTable::SetRow`（**值变才标脏** ⇒ 静态光参数零重传；
         * 逐帧改的光参数随帧槽上行到设备镜像的每一份）。
         */
        class LightSystem : public System
        {
        public:

            LightSystem(const std::string &name = "LightSystem");

            // ── 主方向光（行 0）────────────────────────────────────────────
            const graph::LightRow *GetMainLight() const;
            void SetMainLight(const graph::LightRow &row);

            /// 方向：**指向光源**（与 shader 侧 `main_light.direction_intensity.xyz` 同约定）
            void SetMainLightDirection(const math::Vector3f &to_light);

            void SetMainLightColor(const Color4f &color);
            void SetMainLightIntensity(float intensity);

            /// 角径（度）：软阴影 / 天空日盘共用
            void SetMainLightAngleDeg(float angle_deg);

            /// 时刻模型（`graph::EvaluateSunLight`）→ 行 0 的方向 / 颜色 / 强度（角径不动，
            /// 与旧 `SkyInfo::SetTime` 的覆盖面一致）
            void SetMainLightTime(float hour, float minute, float second);

        private:

            graph::LightRow *EditMainLight();
        };
    }//namespace ecs
}//namespace hgl
