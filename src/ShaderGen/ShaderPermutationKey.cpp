/// ShaderPermutationKey.cpp — 新排列 key → GLSL #define 映射实现

#include<hgl/mtl/LegacyShaderPermutationKey.h>
#include<stdio.h>
#include<hgl/common/RenderOptions.h>

namespace hgl::graph{

void ShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "#define SURFACE_TYPE %d\n"
        "#define SHADOW_MODE %u\n",
        static_cast<int>(GetSurfaceType()),
        static_cast<unsigned>(GetShadowMode()));
    out += buf;

    if (GetTextureArrayMode())
        out += "#define TEXTURE_ARRAY_MODE\n";
}

}//namespace hgl::graph
