#include"Build2DCommon.h"
#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredRectTexture2DArrayBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "RectTexture2DArray";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::RectTexture2DArray);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.is_2d = true;
        bmi.coordinate_system_2d = CoordinateSystem2D::NDC;
        bmi.local_to_world_2d = true;
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::RectTexture2DArray, bmi);
        return true;
    }();
}

ShaderProgramBuildSpec *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
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

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture2DArray] CompileCompositorMaterial failed\n");
    return mci;
}

ShaderProgramBuildSpec *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    (void)definition;
    Material2DCreateConfig cfg(PrimitiveType::Triangles,
                               request.recipe.coordinate_system_2d,
                               request.recipe.local_to_world_2d ? WithLocalToWorld::With : WithLocalToWorld::Without);
    cfg.SetGeometryVertexFormat(request.geometry_vertex_format);
    return CreateRectTexture2DArray(profile, &cfg);
}
}//namespace hgl::graph::mtl
