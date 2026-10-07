#pragma once

#include <hgl/color/Color.h>
#include <hgl/math/Vector.h>
#include <hgl/graph/ubo/LightRow.h>

namespace hgl::graph
{
    using namespace math;

    /**
     * 天空外观（T6b 瘦身：**光源参数已迁出本结构**）。
     *
     * 曾经这里同时住着「天空长什么样」与「主方向光怎么照」（方向 / 颜色 / 强度 / 角径 /
     * 月亮色五个字段），于是光的定义没有单一真源——阴影接收代码各自从天空数据里取方向、
     * 靠"约定一致"维系（见 doc/future/lighting-shadow-domain-split-plan.md §0.3）。
     * T6b 起：
     *   · 光源 = **世界私有的光源行**（`LightRow` 行 0 = 主方向光，地址
     *     `WorldAddresses.addr_lights`）：方向 / 颜色 / 强度 / 角径 / 阴影开关都在那里；
     *   · 本结构只留**天空外观**（底色 / 光晕强度）与 IBL 句柄。
     *     ⚠ 光晕的**颜色**恒为主光色（旧模型里它由时刻模型与主光同步写入、是纯副本），
     *     故不再单独成字段：shader 侧直接读 `main_light.color_angle.rgb`（双源清零）。
     * 时刻模型（太阳方向与色温）随光源走：`graph::EvaluateSunLight(hour, minute, second)`。
     */
    struct SkyInfo
    {
        Color4f     base_sky_color = Color4f(0.1f, 0.3f, 0.6f, 1.0f);
        float       halo_intensity = 0.1f;   ///< 光晕（Mie）强度；**颜色恒取主光色**

        /// **显式填充**（20 → 32）：让 `env_tex` 落在两端写死的 32 字节处。
        /// 为什么不能靠隐式对齐：GLSL 侧是 `scalar` 布局（对齐按**分量**算，`uvec4` 只要求 4），
        /// 而 C++ 侧 GLM 的对齐与类型/编译选项相关 —— 两边都"合理"却可能差 12 字节，
        /// 结果是 shader 拿 `env_tex.x = 0 / y ≠ 0`（越过早退）去采样、IBL 变黑/乱（实测踩过）。
        uint32_t    _pad[3] = { 0, 0, 0 };

        /**
         * IBL 环境贴图 bindless 句柄(1-based,0=未绑定):
         *   x = Diffuse Irradiance Cubemap
         *   y = Specular Prefiltered Cubemap
         *   z = BRDF LUT(2D,(NdotV,roughness) → (scale,bias))
         *   w = 预留
         * 由 IBL ambient 光照模块(light/indirect_ibl.glsl)经 sky.env_tex 采样。
         */
        Vector4u    env_tex        = Vector4u(0);
    };

    // GPU ABI：布局是 C++↔GLSL 的契约（镜像见 ShaderLibrary/ubo/scene_ubo.glsl 的 SkyInfoRef）
    static_assert(sizeof(SkyInfo) == 48, "SkyInfo 必须恰为 48 字节（改字段请同步 GLSL 镜像与断言）");
    static_assert(offsetof(SkyInfo, halo_intensity) == 16, "halo_intensity 必须落在 16B");
    static_assert(offsetof(SkyInfo, env_tex) == 32, "env_tex 必须落在 32B（显式 _pad 保证；"
                                                     "错位 = shader 读到 0 句柄 = IBL 静默失效）");
}
