/// ShaderPermutationKey.cpp — 排列 key → GLSL #define 映射实现

#include<hgl/mtl/ShaderPermutationKey.h>
#include<stdio.h>
#include <string>

namespace hgl::graph::mtl{

/// 宏名称约定
/// -----------
/// ULRE_SKYLIGHT_MODEL   GLSL 小 = C++ 枚举値 + 1（见 SkyLight.h）
///                       Simple→1  FakeAtmosphere→2  CubeMap→3  SH→4  IBL→5
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

void ShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    // SkyLightAmbientModel → ULRE_SKYLIGHT_MODEL_* 数值转换由 SkyLight.h 集中定义
    const uint32_t glsl_skylight = SkyLightAmbientModelToGLSL(ambient);

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
