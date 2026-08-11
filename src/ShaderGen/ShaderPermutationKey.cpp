/// ShaderPermutationKey.cpp — 新排列 key → GLSL #define 映射实现

#include <hgl/shadergen/ShaderPermutationKey.h>
#include<stdio.h>

namespace hgl::graph::shadergen{

void ShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "#define SURFACE_TYPE %d\n"
        "#define SHADOW_MODE %u\n",
        static_cast<int>(GetSurfaceType()),
        static_cast<unsigned>(GetShadowMode()));

    out += buf;
}

}//namespace hgl::graph::shadergen
