/// ShaderPermutationKey.cpp — 新排列 key → GLSL #define 映射实现

#include<hgl/mtl/new/ShaderPermutationKey.h>
#include<stdio.h>
#include<hgl/common/RenderOptions.h>

namespace hgl::graph{

void ShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    char buf[512];

    snprintf(buf, sizeof(buf),
        "#define SURFACE_TYPE %d\n"
        "#define SHADOW_MODE %u\n"
        "#define BASE_TEX_ARRAY_MODE %d\n"
        "#define NORMAL_TEX_ARRAY_MODE %d\n"
        "#define ROUGH_TEX_ARRAY_MODE %d\n"
        "#define TEXTURE_ARRAY_MODE %d\n",
        static_cast<int>(GetSurfaceType()),
        static_cast<unsigned>(GetShadowMode()),
        GetBaseTextureArrayMode() ? 1 : 0,
        GetNormalTextureArrayMode() ? 1 : 0,
        GetRoughnessTextureArrayMode() ? 1 : 0,
        GetTextureArrayMode() ? 1 : 0);

    out += buf;
}

}//namespace hgl::graph
