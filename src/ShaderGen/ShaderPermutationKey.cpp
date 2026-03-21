/// ShaderPermutationKey.cpp — 新排列 key → GLSL #define 映射实现

#include<hgl/mtl/new/ShaderPermutationKey.h>
#include<stdio.h>
#include<hgl/common/RenderOptions.h>

namespace hgl::graph{

void ShaderPermutationKey::AppendGLSLDefines(std::string &out) const
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "#define SURFACE_TYPE %d\n"
        "#define SHADOW_MODE %u\n"
        "#define TEXTURE_ARRAY_MODE %d\n",
        static_cast<int>(GetSurfaceType()),
        static_cast<unsigned>(GetShadowMode()),
        GetTextureArrayMode() ? 1 : 0);
    out += buf;

    for (uint8 i = 0; i < uint8(mtl::SamplerSlotCount); ++i)
    {
        if (GetSlotArrayMode(static_cast<mtl::SamplerSlot>(i)))
        {
            out += "#define TEX_";
            out += mtl::ToUpperASCII(mtl::SamplerSlotNameList[i]);
            out += "_ARRAY 1\n";
        }
    }
}

}//namespace hgl::graph
