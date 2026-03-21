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
    constexpr FixedVertexEntry BILLBOARD_DYNAMIC_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
    };

    constexpr SamplerSlot BILLBOARD_DYNAMIC_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_DYNAMIC_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS) / sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS[0]));
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;

    std::vector<FixedDescriptorEntry> dynamic_descriptors;
    dynamic_descriptors.reserve(4 + BILLBOARD_DYNAMIC_TEX_SLOT_COUNT);
    dynamic_descriptors.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));
    dynamic_descriptors.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::CameraInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));
    dynamic_descriptors.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));
    dynamic_descriptors.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::TransformID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)));

    for (uint32_t i = 0; i < BILLBOARD_DYNAMIC_TEX_SLOT_COUNT; ++i)
        dynamic_descriptors.push_back(MakeTextureDescriptorEntry(BILLBOARD_DYNAMIC_TEX_SLOTS[i],
                                                                 uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                                 TextureSourceMode::Simple));

    FixedMaterialDef dynamic_def {
        "BillboardDynamic",
        PrimitiveType::Triangles,
        BILLBOARD_DYNAMIC_VERTEX,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_VERTEX) / sizeof(BILLBOARD_DYNAMIC_VERTEX[0])),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        nullptr,
        0,
    };

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardCameraFacing;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.SetHasTexture(SamplerSlot::BaseColor);
    var_key.blend_mode          = BlendMode::Transparent;
    var_key.pass_hint           = PassType::ForwardTransparent;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardDynamic] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardDynamic] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[BillboardDynamic] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
