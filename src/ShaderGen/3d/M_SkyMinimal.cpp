#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredSkyMinimalBmi = []() -> bool
    {
        BaseMaterialInfo bmi{};
        bmi.bmi_name = "SkyMinimal";
        bmi.shading_model = ShadingModel::Sky;
        bmi.preset_hint = static_cast<uint32_t>(MaterialPreset::SkyMinimal);
        RegisterBaseMaterialInfo(MaterialPreset::SkyMinimal, bmi);
        return true;
    }();

    constexpr FixedDescriptorEntry SKY_MINIMAL_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
    };

}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg)
{
    FixedVertexEntry sky_minimal_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
    };

    FixedMaterialDef dynamic_def {
        "SkyMinimal",
        PrimitiveType::Triangles,
        sky_minimal_vertex,
        uint32_t(sizeof(sky_minimal_vertex) / sizeof(sky_minimal_vertex[0])),
        SKY_MINIMAL_DESCRIPTORS,
        uint32_t(sizeof(SKY_MINIMAL_DESCRIPTORS) / sizeof(SKY_MINIMAL_DESCRIPTORS[0])),
        nullptr,
        0,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Sky,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_sky.vert.glsl",
        "compositor/main_forward_sky.frag.glsl",
        "surface/sky_minimal_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[SkyMinimal] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[SkyMinimal] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
