/// ShaderPermutationKey.cpp — 排列 key → GLSL #define 映射实现

#include<hgl/graph/mtl/FixedMaterialDef.h>
#include<hgl/type/String.h>
#include<stdio.h>

namespace hgl::graph::mtl{

/// 宏名称约定
/// -----------
/// AMBIENT_MODEL   0=FLAT_COLOR  1=HEMISPHERE  2=IBL  3=IBL_SH  4=MIXED_GI
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
///   #if AMBIENT_MODEL >= 2  // IBL 及以上
///     uniform samplerCube env_map;
///   #endif

void ShaderPermutationKey::AppendGLSLDefines(AnsiString &out) const
{
    char buf[256];

    snprintf(buf,sizeof(buf),
        "#define AMBIENT_MODEL %u\n"
        "#define LIGHT_MODEL %u\n"
        "#define SPECULAR_SPLIT %u\n"
        "#define SHADOW_MODE %u\n",
        (unsigned)ambient,
        (unsigned)light,
        (unsigned)specular,
        (unsigned)shadow);

    out += buf;
}

}//namespace hgl::graph::mtl
