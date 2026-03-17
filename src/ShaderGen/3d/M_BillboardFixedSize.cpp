#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<cstdio>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uvec2 BillboardSize;";
    constexpr const uint32_t mi_bytes=sizeof(uint32_t)*2;       // uvec2 = 2 x uint32

    constexpr FixedVertexEntry BILLBOARD_FIXED_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
    };

    constexpr FixedDescriptorEntry BILLBOARD_FIXED_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",     nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",       nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material,  MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_VERTEX_BIT),   "mtl",      "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
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

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardAxisLocked;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.feature_bits        = VF_HasBaseColorTex;
    var_key.blend_mode          = BlendMode::Transparent;
    var_key.pass_hint           = PassType::ForwardTransparent;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardFixed] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, PlatformBackend::PC, *var_desc);

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
