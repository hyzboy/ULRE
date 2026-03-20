#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>
#include<vector>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uvec2 BillboardSize;";
    constexpr const uint32_t mi_bytes=sizeof(uint32_t)*2;       // uvec2 = 2 x uint32

    constexpr FixedVertexEntry BILLBOARD_FIXED_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
    };

    // Non-texture descriptors �?texture entries built dynamically.
    constexpr FixedDescriptorEntry BILLBOARD_FIXED_BASE_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",           nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",             nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",      "LocalToWorldData",       nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT),   "tid",      "TransformIDData",        nullptr },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT),   "mid",      "MaterialInstanceIDData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT),   "mtl",      "MaterialInstanceData",   nullptr },
    };

    constexpr SamplerName::SamplerSlot BILLBOARD_FIXED_TEX_SLOTS[] = {
        SamplerName::SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_FIXED_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_FIXED_TEX_SLOTS) / sizeof(BILLBOARD_FIXED_TEX_SLOTS[0]));

    constexpr FixedMaterialDef BILLBOARD_FIXED_DEF_TEMPLATE {
        "BillboardFixed",
        PrimitiveType::Triangles,
        BILLBOARD_FIXED_VERTEX,
        uint32_t(sizeof(BILLBOARD_FIXED_VERTEX) / sizeof(BILLBOARD_FIXED_VERTEX[0])),
        BILLBOARD_FIXED_BASE_DESCRIPTORS,
        uint32_t(sizeof(BILLBOARD_FIXED_BASE_DESCRIPTORS) / sizeof(BILLBOARD_FIXED_BASE_DESCRIPTORS[0])),
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

    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        BILLBOARD_FIXED_BASE_DESCRIPTORS,
        BILLBOARD_FIXED_BASE_DESCRIPTORS + uint32_t(sizeof(BILLBOARD_FIXED_BASE_DESCRIPTORS) / sizeof(BILLBOARD_FIXED_BASE_DESCRIPTORS[0])));
    for (uint32_t i = 0; i < BILLBOARD_FIXED_TEX_SLOT_COUNT; ++i)
        dynamic_descriptors.push_back({
            DescriptorSetType::Material, DescriptorKind::TextureSampler,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            SamplerName::ToDescriptorName(BILLBOARD_FIXED_TEX_SLOTS[i]),
            nullptr, "sampler2D"
        });

    FixedMaterialDef dynamic_def = BILLBOARD_FIXED_DEF_TEMPLATE;
    dynamic_def.descriptor_entries     = dynamic_descriptors.data();
    dynamic_def.descriptor_entry_count = uint32_t(dynamic_descriptors.size());

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardAxisLocked;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.SetHasTexture(SamplerSlot::BaseColor);
    var_key.blend_mode          = BlendMode::Transparent;
    var_key.pass_hint           = PassType::ForwardTransparent;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardFixed] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardFixed] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[BillboardFixed] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
