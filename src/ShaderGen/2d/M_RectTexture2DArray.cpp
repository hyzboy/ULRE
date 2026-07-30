#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DArrayBmi = []() -> bool
    {
        BaseMaterialInfo bmi{};
        bmi.bmi_name = "RectTexture2DArray";
        bmi.preset_hint = static_cast<uint32_t>(MaterialPreset::RectTexture2DArray);
        bmi.source_kind = BMISourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        RegisterBaseMaterialInfo(MaterialPreset::RectTexture2DArray, bmi);
        return true;
    }();
}

MaterialCreateInfo *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    constexpr const char mi_codes[]="uvec4 id;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4u);
    if(!cfg)
        return(nullptr);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=true;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;
    const VkFormat position_format = ResolveMaterialPositionFormat(cfg, VK_FORMAT_R32G32_SFLOAT);

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner, true, true, position_format);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner, position_format);
    build2d::PushSemanticVertexEntry(vertices, &inner, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT);

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2DArray", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler});

    FixedMaterialDef def {
        "RectTexture2DArray",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/recttexture2darray.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/recttexture2darray.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture2DArray] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
