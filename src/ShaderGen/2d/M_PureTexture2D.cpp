#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureTexture2D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        RegisterBaseMaterialInfo(BuiltinMaterialCreatorID::PureTexture2D, bmi);
        return true;
    }();
}

ShaderProgramBuildSpec *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    auto preamble = build2d::Build2DPreamble(cfg, true, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    build2d::PushSemanticVertexEntry(vertices, cfg, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2D", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler});

    FixedMaterialDef def {
        "PureTexture2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = preamble + "#include \"2d/puretexture2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl


