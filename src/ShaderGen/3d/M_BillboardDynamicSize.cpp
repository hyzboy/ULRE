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
    constexpr FixedVertexEntry BILLBOARD_DYNAMIC_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    };

    constexpr FixedDescriptorEntry BILLBOARD_DYNAMIC_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",     nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",       nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
    };

    constexpr FixedMaterialDef BILLBOARD_DYNAMIC_DEF {
        "BillboardDynamic",
        PrimitiveType::Triangles,
        BILLBOARD_DYNAMIC_VERTEX,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_VERTEX) / sizeof(BILLBOARD_DYNAMIC_VERTEX[0])),
        BILLBOARD_DYNAMIC_DESCRIPTORS,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_DESCRIPTORS) / sizeof(BILLBOARD_DYNAMIC_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardCameraFacing;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.feature_bits        = VF_HasBaseColorTex;
    var_key.blend_mode          = BlendMode::Transparent;
    var_key.pass_hint           = PassType::ForwardTransparent;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardDynamic] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, PlatformBackend::PC, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardDynamic] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        BILLBOARD_DYNAMIC_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BillboardDynamic] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
