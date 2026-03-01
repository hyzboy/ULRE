/// ShaderPermutationKey.cpp — 排列 key → GLSL #define 映射实现

#include<hgl/graph/mtl/FixedMaterialDef.h>
#include<hgl/type/String.h>
#include<stdio.h>

namespace hgl::graph::mtl{

/// 宏名称约定
/// -----------
/// ULRE_SKYLIGHT_MODEL   Simple→1  IBL→2  SphericalHarmonics→4
/// LIGHT_MODEL     0=UNLIT  1=LAMBERT  2=BLINN_PHONG  3=PBR_LITE  4=PBR_FULL  5=CEL_SHADING
/// SPECULAR_SPLIT  0=COMBINED  1=SEPARATED
/// SHADOW_MODE     0=NONE  1=PCF  2=PCSS
///
/// GLSL 侧用法示例（fragment shader 头部）：
///
///   #if LIGHT_MODEL == 2   // BLINN_PHONG
///     #include "blinnphong_lighting.glsl"  // 或内联实现
///   #elif LIGHT_MODEL == 3  // PBR_LITE
///     #include "pbr_lite_lighting.glsl"
///   #endif
///
///   #if ULRE_SKYLIGHT_MODEL >= ULRE_SKYLIGHT_MODEL_IBL
///     uniform samplerCube env_map;
///   #endif

void ShaderPermutationKey::AppendGLSLDefines(AnsiString &out) const
{
    // SkyLightAmbientModel 到 ULRE_SKYLIGHT_MODEL_* 数字的映射表
    // Simple=0 →1(SIMPLE)  IBL=1 →2(IBL)  SphericalHarmonics=2 →4(SH)
    static const uint32_t SKYLIGHT_GLSL_VALUES[] = { 1, 2, 4 };
    const uint32_t glsl_skylight = SKYLIGHT_GLSL_VALUES[
        unsigned(ambient) < sizeof(SKYLIGHT_GLSL_VALUES)/sizeof(SKYLIGHT_GLSL_VALUES[0])
        ? unsigned(ambient) : 0];

    char buf[256];
    snprintf(buf,sizeof(buf),
        "#define ULRE_SKYLIGHT_MODEL %u\n"
        "#define LIGHT_MODEL %u\n"
        "#define SPECULAR_SPLIT %u\n"
        "#define SHADOW_MODE %u\n",
        glsl_skylight,
        (unsigned)light,
        (unsigned)specular,
        (unsigned)shadow);

    out += buf;
}

}//namespace hgl::graph::mtl
