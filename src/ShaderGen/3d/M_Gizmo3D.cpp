#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<string>

namespace hgl::graph::mtl
{
namespace
{
    const bool kRegisteredGizmo3DBmi = []() -> bool
    {
        BaseMaterialInfo bmi{};
        bmi.bmi_name = "Gizmo3D";
        bmi.shading_model = ShadingModel::Unlit;
        bmi.preset_hint = static_cast<uint32_t>(MaterialPreset::Gizmo3D);
        RegisterBaseMaterialInfo(MaterialPreset::Gizmo3D, bmi);
        return true;
    }();

    // ─────────────────────────────────────────────────────────────────────────────
    // 顶点输入和描述符定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr FixedDescriptorEntry GIZMO_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::PBRSurface, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind) },
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // 材质实例定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
    constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

}

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    // 通过 CompositorAssembler 从 .glsl 模板文件组装 VS/FS
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_unlit_normal.vert.glsl",   // VS: Pos+Normal+TID+MIID
        "compositor/main_forward_unlit_normal.frag.glsl",   // FS: worldPos+worldNormal+MIID + camera
        "surface/gizmo3d_surface.glsl"                      // Surface: MI color + Blinn-Phong
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[Gizmo3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    if(cfg)
        cfg->material_instance=true;

    FixedVertexEntry gizmo_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(cfg, VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Normal },
    };

    FixedMaterialDef dynamic_def {
        "Gizmo3D",
        PrimitiveType::Triangles,
        gizmo_3d_vertex,
        uint32_t(sizeof(gizmo_3d_vertex) / sizeof(gizmo_3d_vertex[0])),
        GIZMO_3D_DESCRIPTORS,
        uint32_t(sizeof(GIZMO_3D_DESCRIPTORS) / sizeof(GIZMO_3D_DESCRIPTORS[0])),
        GIZMO_3D_MI_GLSL,
        GIZMO_3D_MI_BYTES,
    };

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[Gizmo3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
