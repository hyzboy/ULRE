#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uvec2 BillboardSize;";
    constexpr const uint32_t mi_bytes=sizeof(uint32_t)*2;       // uvec2 = 2 x uint32

    constexpr FixedVertexEntry BILLBOARD_FIXED_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    constexpr FixedDescriptorEntry BILLBOARD_FIXED_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",     nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",       nullptr, DescriptorSemantic::CameraInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO},
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(TransformDescriptorKind) },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material,  MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_VERTEX_BIT),   "mtl",      "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::PBRSurface, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind) },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO},
        { DescriptorSetType::Material,  DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D", DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Texture},
    };

    constexpr FixedMaterialDef BILLBOARD_FIXED_DEF {
        "BillboardFixed",
        PrimitiveType::Triangles,
        BILLBOARD_FIXED_VERTEX,
        uint32_t(sizeof(BILLBOARD_FIXED_VERTEX) / sizeof(BILLBOARD_FIXED_VERTEX[0])),
        BILLBOARD_FIXED_DESCRIPTORS,
        uint32_t(sizeof(BILLBOARD_FIXED_DESCRIPTORS) / sizeof(BILLBOARD_FIXED_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
    };
}//namespace

MaterialCreateInfo *CreateBillboard2DFixedSize(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;
    cfg->material_instance=true;

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Unlit,
        BlendMode::Transparent,
        PassType::ForwardTransparent,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_billboard_fixed.vert.glsl",
        "compositor/main_forward_billboard.frag.glsl",
        "surface/billboard_texture_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardFixed] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        BILLBOARD_FIXED_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BillboardFixed] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl


