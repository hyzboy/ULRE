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
     * `ShaderLibrary/module/provider/ambient_light/ambient_light_ibl.glsl` —— 一个 `@ulre slot ambient_light_provider`
     * 模块，与天空外观（`sky_provider`）本就是**两个不同的 provider 槽**。
     * 它原先住在 `SkyInfo` 里（Scene 集时代的历史遗留），也正是那里必须挂显式填充写死偏移的原因：
     * 一端 16 对齐、一端 scalar 布局 ⇒ 句柄被挤到不同偏移 ⇒ shader 拿句柄 0 采样 ⇒ IBL 静默发黑。
     *
     * 块内字段全为 `16 字节整数倍` 对齐的类型（`uvec4` / `vec4` / `vec4[9]`）⇒ 两端都落在 16 的倍数上，
     * 布局不可能因隐式对齐分歧，显式填充与偏移类断言都不再需要（这正是 R2 的治法：治根，不是兜底）。
     *
     * 三种间接光算法（`ambient_light_ibl` / `ambient_light_ibl_oct` / `ambient_light_sh`）共用本块：
     * 前两者读 `env_tex`（+ 可选 `ldr_param`），后者读 `sh`。未使用的字段恒为 0 ⇒ 互不干扰。
     *
     * 规则出处：doc/future/sky-domain-and-provider-params-plan.md §2.1（R1 一槽一块一地址 / R2 同族字段）
     * 与 §2.2（本块 = `WorldAddresses.addr_ambient_env`，单份 buffer ⇒ 全帧槽同址）。
     */
    struct HGL_UBO_BDA AmbientEnvInfo
    {
        /**
         * IBL 环境贴图 bindless 句柄(1-based,0=未绑定):
         *   x = Diffuse Irradiance Cubemap
         *   y = Specular Prefiltered Cubemap(带粗糙度 mip 链)
         *   z = BRDF LUT(2D,(NdotV,roughness) → (scale,bias))
         *   w = Prefiltered Cubemap 的 mip 级数(0/1 = 只有一级,退化为固定粗糙度)
         * 由 IBL ambient 光照模块(module/provider/ambient_light/ambient_light_ibl.glsl)经 ambient_env.env_tex 采样。
         *
         * w 是 shader 把 roughness 换算成 LOD 的唯一依据:lod = roughness * (w-1)，与烘焙端
         * TexConv `PrefilterRoughness` 的「第 i 级 = i/(级数-1)」互逆，两端必须同时改。
         */
        Vector4u    env_tex = Vector4u(0);

        /**
         * LDR IBL 曝光与太阳光强度动态调制参数：
         *   x = is_ldr (0.0 = HDR/关闭调制, >0.0 = LDR 模式生效)
         *   y = exposure_base (基准曝光倍率，默认建议 2.5f ~ 3.0f)
         *   z = sun_scale (太阳高度角与光强调制权重，默认 1.0f)
         *   w = min_ambient (夜间 / 太阳落山后的最低环境光保底比例，默认 0.05f)
         */
        Vector4f    ldr_param = Vector4f(0.0f);

        /**
         * L2 球谐（SH9）天光系数，Ramamoorthi & Hanrahan 漫反射辐照度表示：
         *   `sh[i].xyz` = 第 i 个基函数的 RGB 系数，`sh[i].w` 保留恒 0。
         *
         * 下标 → 基函数（Z-up，本引擎世界坐标约定）：
         *   0 = Y00 常量             1 = Y1-1 ∝ y        2 = Y10 ∝ z
         *   3 = Y11 ∝ x              4 = Y2-2 ∝ x*y      5 = Y2-1 ∝ y*z
         *   6 = Y20 ∝ 3z²-1          7 = Y21 ∝ x*z       8 = Y22 ∝ x²-y²
         *
         * 读取方唯一：module/provider/ambient_light/ambient_light_sh.glsl。
         * 写入方为 CPU（逐帧按太阳方向/强度与大气色合成；全 0 = 无天光，与未布探针等价）。
         * 顺序一旦改变，着色器与 CPU 两侧必须同时改。
         */
        Vector4f    sh[9] = {};
    };

    // GPU ABI：布局是 C++↔GLSL 的契约（镜像见 ShaderLibrary/module/descriptor/ubo/scene_ubo.glsl 的 AmbientEnvInfoRef）
    static_assert(sizeof(AmbientEnvInfo) == 176, "AmbientEnvInfo 必须恰为 176 字节（uvec4 env_tex + vec4 ldr_param + vec4 sh[9]）");
    static_assert(offsetof(AmbientEnvInfo, env_tex) == 0, "env_tex 必须是块首字段（两端同型 uvec4）");
    static_assert(offsetof(AmbientEnvInfo, ldr_param) == 16, "ldr_param 必须在偏移 16");
    static_assert(offsetof(AmbientEnvInfo, sh) == 32, "sh 必须在偏移 32（vec4 数组无隐式填充）");
}
