/// NewShaderPermutationKey.cpp — 新排列 key → GLSL #define 映射实现

#include<hgl/mtl/new/NewShaderPermutationKey.h>
#include<stdio.h>

namespace hgl::graph{

void NewShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    char buf[512];
    snprintf(buf, sizeof(buf),
        "#define SURFACE_TYPE %d\n"
        "#define SHADOW_MODE %u\n"
        "#define GEOMETRY_FETCH_SSBO %d\n",
        static_cast<int>(GetSurfaceType()),
        static_cast<unsigned>(GetShadowMode()),
        // geometry fetch mode: SSBO geometry fetch not yet implemented in renderer,
        // always 0 until VertexDataBuffer SSBO pipeline is wired up.
        0);

    out += buf;
}

}//namespace hgl::graph
