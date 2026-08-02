#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureTexture2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureTexture2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureTexture2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        bmi.with_local_to_world = true;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.texture_slot_decls = {{TextureSlot::BaseColor, GLSLSamplerType::Sampler2D, true}};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureTexture2D, bmi);
        return true;
    }();
}

static ShaderProgramBuildSpec *CreatePureTexture2DImpl(const contract::PhysicalDeviceProfileLite *profile, const Material2DBuildParams &p)
{
    auto preamble = build2d::Build2DPreamble(p, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p);
    build2d::PushSemanticVertexEntry(vertices, p, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "PureTexture2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = preamble + "#include \"2d/puretexture2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[PureTexture2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreatePureTexture2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
