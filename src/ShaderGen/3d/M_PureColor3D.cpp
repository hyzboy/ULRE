#include<hgl/shadergen/ShaderProgramBuildSpec.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<string>

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredPureColor3DBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "PureColor3D";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::PureColor3D);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = false;
        bmi.usage_tag   = MaterialDefinitionUsageTag::Fallback;
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::PureColor3D, bmi);

        // builtin/fallback_3d: 3D 无材质保底
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_FALLBACK_3D;
            alias.definition_name = "builtin/fallback_3d";
            RegisterMaterialDefinition(alias);
        }

        // builtin/missing_material: 缺失材质（当前与 fallback_3d 相同，未来可换成棋盘格）
        {
            MaterialDefinition alias = bmi;
            alias.definition_id = BUILTIN_MTL_DEF_MISSING_MATERIAL;
            alias.definition_name = "builtin/missing_material";
            RegisterMaterialDefinition(alias);
        }

        return true;
    }();

    constexpr const char pure_color_3d_mi_codes[] = "vec4 Color;";
    constexpr const uint32_t pure_color_3d_mi_bytes = 16;

    constexpr FixedDescriptorEntry PURE_COLOR_3D_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",   nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material,  MaterialInstanceDescriptorKind,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::PBRSurface, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind) },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, GetMaterialStructSlotIndex(SSBOType::PBRSurface), SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
    };

}

static ShaderProgramBuildSpec *CreatePureColor3DImpl(const contract::PhysicalDeviceProfileLite *profile, const CompositorMaterialBuildConfig &bc)
{
    FixedVertexEntry pure_color_3d_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
    };

    FixedMaterialDef dynamic_def {
        "PureColor3D",
        PrimitiveType::Triangles,
        pure_color_3d_vertex,
        uint32_t(sizeof(pure_color_3d_vertex) / sizeof(pure_color_3d_vertex[0])),
        PURE_COLOR_3D_DESCRIPTORS,
        uint32_t(sizeof(PURE_COLOR_3D_DESCRIPTORS) / sizeof(PURE_COLOR_3D_DESCRIPTORS[0])),
        pure_color_3d_mi_codes,
        pure_color_3d_mi_bytes,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[PureColor3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        bc);

    if (!mci)
        std::fprintf(stderr, "[PureColor3D] CompileCompositorMaterial failed\n");
    return mci;
}

ShaderProgramBuildSpec *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    CompositorMaterialBuildConfig bc = ToCompositorBuildConfig3D(request, definition, true);
    return CreatePureColor3DImpl(profile, bc);
}
}//namespace hgl::graph::mtl
