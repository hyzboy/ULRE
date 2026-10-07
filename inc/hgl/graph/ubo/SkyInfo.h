#pragma once

#include <hgl/color/Color.h>
#include <hgl/math/Vector.h>
#include <cstddef>

namespace hgl::graph
{
    using namespace math;

    /**
     * 天空外观 = `sky_provider` 槽的参数块（**块内只有外观标量**）。
     *
     * 演化（三块各自的归属，见 doc/future/sky-domain-and-provider-params-plan.md §2.1 R1）：
     *   · 光源参数（方向 / 颜色 / 强度 / 角径 / 阴影开关）在**世界私有光源行**
     *     （`LightRow` 行 0 = 主方向光，地址 `WorldAddresses.addr_lights`；T6b 迁出）；
     *   · IBL 句柄已归位到它真正的消费者槽 —— `AmbientEnvInfo`
     *     （`ambient_light_provider`，地址 `WorldAddresses.addr_ambient_env`；T1 迁出）；
     *   · 本块回到**同族字段**（颜色 + 强度标量）⇒ 不再需要显式填充、不再需要句柄偏移断言。
     *
     * R2（块内只放同族字段；句柄类单独成块）之所以是硬规则：句柄寄生在外观结构里时，
     * 两端只要对「uvec4 的对齐」理解不同（C++ GLM 16 对齐 vs GLSL scalar 只要求 4），
     * 句柄就会落到不同偏移 ⇒ shader 拿句柄 0 采样 ⇒ 模块静默发黑（实测事故）。
     *
     * ⚠ 光晕的**颜色**恒为主光色（旧模型里它由时刻模型与主光同步写入、是纯副本），
     * 故不单独成字段：shader 侧直接读 `main_light.color_angle.rgb`（双源清零）。
     * 时刻模型（太阳方向与色温）随光源走：`graph::EvaluateSunLight(hour, minute, second)`。
     */
    struct SkyInfo
    {
        Color4f     base_sky_color = Color4f(0.1f, 0.3f, 0.6f, 1.0f);
        float       halo_intensity = 0.1f;   ///< 光晕（Mie）强度；**颜色恒取主光色**
    };

    // GPU ABI：布局是 C++↔GLSL 的契约（镜像见 ShaderLibrary/ubo/scene_ubo.glsl 的 SkyInfoRef）
    //
    // 有效数据 = 20 B（vec4 + float）；本工程定义 `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES`
    // （CMCMakeModule/math.cmake）⇒ `Color4f`（glm::vec4 派生）16 对齐 ⇒ sizeof 向上取整到 32
    // （尾部 12 B 填充；**其后没有字段，故两端不必对这个填充达成任何契约**）。
    // 真正的契约只有字段偏移：两端 `base_sky_color@0` / `halo_intensity@16`。
    static_assert(sizeof(SkyInfo) == 32, "SkyInfo = 20 B 数据 + 12 B 尾部对齐填充（改字段请同步 GLSL 镜像）");
    static_assert(offsetof(SkyInfo, halo_intensity) == 16, "halo_intensity 必须落在 16B（两端同偏移）");
}
