#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/math/Vector.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::UserDefined}};
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor2D, bmi);

        // Register builtin alias so fallback code can look up by canonical id.
        MaterialDefinition fallback_alias = bmi;
        fallback_alias.definition_id = BUILTIN_MTL_DEF_FALLBACK_2D;
        fallback_alias.definition_name = "builtin/fallback_2d";
        RegisterMaterialDefinition(fallback_alias);

        return true;
    }();
}

static ShaderProgramBuildSpec *CreatePureColor2DImpl(const contract::PhysicalDeviceProfileLite *profile, Material2DBuildParams p)
{
    constexpr const char mi_codes[]="vec4 Color;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);

    p.material_instance = true;

    auto preamble = build2d::Build2DPreamble(p, false, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, p);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, p);

    FixedMaterialDef def {
        "PureColor2D",
        p.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/purecolor2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/purecolor2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        std::fprintf(stderr, "[PureColor2D] CompileCompositorMaterial failed\n");
    return mci;
}

ShaderProgramBuildSpec *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    (void)definition;
    return CreatePureColor2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
