#pragma once

#include <hgl/graph/BufferAnnotation.h>
#include <hgl/math/Vector.h>
#include <cstddef>

namespace hgl::graph
{
    using namespace math;

    /**
     * 环境光 provider 槽（`ambient_light_provider`）的参数块。
     *
     * 为什么独立成块（T1 归位）：块里的句柄的**唯一读者**是
     * `ShaderLibrary/lighting/indirect_ibl.glsl` —— 一个 `@ulre slot ambient_light_provider`
     * 模块，与天空外观（`sky_provider`）本就是**两个不同的 provider 槽**。
     * 它原先住在 `SkyInfo` 里（Scene 集时代的历史遗留），也正是那里必须挂显式填充写死偏移的原因：
     * 一端 16 对齐、一端 scalar 布局 ⇒ 句柄被挤到不同偏移 ⇒ shader 拿句柄 0 采样 ⇒ IBL 静默发黑。
     *
     * 块内**只有句柄**（同族字段）：`uvec4` 在两端都落在 16 的倍数上 ⇒ 布局不可能因隐式对齐分歧，
     * 显式填充与偏移类断言都不再需要（这正是 R2 的治法：治根，不是兜底）。
     *
     * 规则出处：doc/future/sky-domain-and-provider-params-plan.md §2.1（R1 一槽一块一地址 / R2 同族字段）
     * 与 §2.2（本块 = `WorldAddresses.addr_ambient_env`，单份 buffer ⇒ 全帧槽同址）。
     */
    struct HGL_UBO_BDA AmbientEnvInfo
    {
        /**
         * IBL 环境贴图 bindless 句柄(1-based,0=未绑定):
         *   x = Diffuse Irradiance Cubemap
         *   y = Specular Prefiltered Cubemap
         *   z = BRDF LUT(2D,(NdotV,roughness) → (scale,bias))
         *   w = 预留
         * 由 IBL ambient 光照模块(lighting/indirect_ibl.glsl)经 ambient_env.env_tex 采样。
         */
        Vector4u    env_tex = Vector4u(0);
    };

    // GPU ABI：布局是 C++↔GLSL 的契约（镜像见 ShaderLibrary/ubo/scene_ubo.glsl 的 AmbientEnvInfoRef）
    static_assert(sizeof(AmbientEnvInfo) == 16, "AmbientEnvInfo 必须恰为 16 字节（块内只有 uvec4）");
    static_assert(offsetof(AmbientEnvInfo, env_tex) == 0, "env_tex 必须是块首字段（两端同型 uvec4）");
}
