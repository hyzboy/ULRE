#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/log/Log.h>
#include<hgl/math/Vector.h>
#include "../common/VertexShaderAssembler.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor2DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor2D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor2D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.ubo_requirements = {UBODescriptorSemantic::ViewportInfo};
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        bmi.ssbo_slot_decls = {{"mtl", SSBOType::EmissiveSurface}};
        bmi.vertex_node_config = Make2DNodeConfigNDC(true);
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

    auto preamble = build2d::Build2DFragmentPreamble(p, false);

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

    const VkFormat position_format = ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);
    VertexVaryingConfig varying_cfg;
    varying_cfg.emit_data_index_id = true;
    std::string vs = GenerateVertexShader(
        p.vertex_node_config,
        varying_cfg,
        position_format,
        "",
        "ShaderLibrary"
    );
    std::string fs = preamble + "#include \"2d/purecolor2d.frag.glsl\"\n";

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, build2d::ToCompositorBuildConfig2D(p));
    if(!mci)
        GLogError("[PureColor2D] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreatePureColor2DImpl(profile, Material2DBuildParams::From(request, definition));
}
}//namespace hgl::graph::mtl
