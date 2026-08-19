// @ulre begin
// @ulre name indirect_sky_ambient
// @ulre kind Utility
// @ulre priority 0
// @ulre uses lighting_interface
// @ulre uses sky_info
// @ulre uses sky_atmosphere
// @ulre end
// Indirect Lighting — Sky Light Ambient（低画质间接光）
// 与天空球同一 sky light 算法（EvalSkyAtmosphere）：以物体世界法线为方向采样
// 天空色当间接光——无 GI/IBL 探针时的廉价替代（方向相关——法线朝上→天空色、
// 朝下→地面色——比常量 ambient 真实）。替换 indirect_simple_ambient。
#ifndef INDIRECT_SKY_AMBIENT_GLSL
#define INDIRECT_SKY_AMBIENT_GLSL

#include "common/lighting_interface.glsl"
#include "ubo/sky_info.glsl"
#include "sky/sky_atmosphere.glsl"

vec3 EvalIndirectLighting(
    LightingInput lighting
) {
    // 世界法线方向采样天空（下半球方向采样到地面色——EvalSkyAtmosphere 对
    // dir.z<0 会退到地平线色——天然的地面反照近似）
    vec3 sky_light = EvalSkyAtmosphere(normalize(lighting.normal));

    return sky_light * lighting.baseColor
         * (1.0 - lighting.metallic) * lighting.ao;
}

#endif // INDIRECT_SKY_AMBIENT_GLSL
