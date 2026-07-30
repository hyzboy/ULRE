#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/math/Vector.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor2DBmi = []() -> bool
    {
        BaseMaterialInfo bmi{};
        bmi.bmi_name = "PureColor2D";
        bmi.preset_hint = static_cast<uint32_t>(MaterialPreset::PureColor2D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        bmi.usage_tag   = BMIUsageTag::Fallback;
        RegisterBaseMaterialInfo(MaterialPreset::PureColor2D, bmi);

        // Register builtin alias so fallback code can look up by canonical id.
        BaseMaterialInfo fallback_alias = bmi;
        fallback_alias.bmi_id = BUILTIN_BMI_FALLBACK_2D;
        fallback_alias.bmi_name = "builtin/fallback_2d";
        RegisterBaseMaterialInfo(fallback_alias);

        return true;
    }();
}

MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,Material2DCreateConfig *cfg)
{
    constexpr const char mi_codes[]="vec4 Color;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);

    cfg->material_instance=true;

    auto preamble = build2d::Build2DPreamble(cfg, false, true);

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);

    FixedMaterialDef def {
        "PureColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
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
