#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/math/Vector.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,Material2DCreateConfig *cfg)
{
    constexpr const char mi_codes[]="vec4 Color;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);

    cfg->material_instance=true;

    auto preamble = build2d::Build2DPreamble(cfg, false, true);

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    build2d::PushBaseUBODescriptors(ubos, cfg);
    build2d::PushBaseSSBODescriptors(ssbos, cfg);

    FixedMaterialDef def {
        "PureColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        nullptr,
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/purecolor2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/purecolor2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureColor2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
