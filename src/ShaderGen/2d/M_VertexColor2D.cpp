#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredVertexColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.bmi_name = "VertexColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::VertexColor2D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        RegisterBaseMaterialInfo(BuiltinMaterialCreatorID::VertexColor2D, bmi);
        return true;
    }();
}

ShaderProgramBuildSpec *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const Material2DCreateConfig *cfg)
{
    auto preamble = build2d::Build2DPreamble(cfg, false, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    build2d::PushSemanticVertexEntry(vertices, cfg, VertexSemantic::Color, VK_FORMAT_R32G32B32A32_SFLOAT);

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

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[VertexColor2D] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl

