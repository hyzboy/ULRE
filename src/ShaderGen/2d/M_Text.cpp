#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredText2DBmi = []() -> bool
    {
        BaseMaterialInfo bmi{};
        bmi.bmi_name = "Text2D";
        bmi.preset_hint = static_cast<uint32_t>(MaterialPreset::Text2D);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.usage_tag = BMIUsageTag::Text;
        bmi.coordinate_system_2d = CoordinateSystem2D::Ortho;
        bmi.local_to_world_2d = false;
        RegisterBaseMaterialInfo(MaterialPreset::Text2D, bmi);

        BaseMaterialInfo alias = bmi;
        alias.bmi_id = BUILTIN_BMI_TEXT;
        alias.bmi_name = "builtin/text";
        RegisterBaseMaterialInfo(alias);

        return true;
    }();
}

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const Text2DMaterialCreateConfig *cfg)
{
    constexpr const char mi_codes[]="uint TextColor;";
    constexpr const uint32_t mi_bytes=sizeof(uint32);
    if(!profile||!cfg)
        return(nullptr);

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;
    const VkFormat position_format = ResolveMaterialPositionFormat(cfg, VK_FORMAT_R32G32_SINT);

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&new_cfg, true, true, position_format);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg, position_format);
    build2d::PushSemanticVertexEntry(vertices, &new_cfg, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &new_cfg);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::Text, nullptr, "sampler2D", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler});

    FixedMaterialDef def {
        "Text2D",
        new_cfg.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/text2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/text2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
