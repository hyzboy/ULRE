#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const Material2DCreateConfig *cfg)
{
    auto preamble = build2d::Build2DPreamble(cfg, false, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({ VK_FORMAT_R32G32B32A32_SFLOAT, VertexSemantic::Color });

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);

    FixedMaterialDef def {
        "VertexColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = preamble + "#include \"2d/vertexcolor2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/vertexcolor2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[VertexColor2D] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl

