#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/new/MaterialVariantKey.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateRectTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    const bool has_slot_mode = key.HasAnyTextureSourceBits();
    const TextureSourceMode mode = has_slot_mode
        ? key.GetTextureSourceMode(SamplerSlot::BaseColor)
        : key.texture_source_mode;
    const bool use_array = (mode == TextureSourceMode::Array);

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(key,nullptr);
    if (!var_desc)
    {
        std::fprintf(stderr, "[RectTexture2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    // MI 结构：Array 模式需要 MI 数据（用于 MIT SSBO）
    constexpr const char mi_codes[] = "uvec4 id;";
    constexpr const uint32_t mi_bytes = sizeof(math::Vector4u);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=use_array;  // Array 模式需要 MI
    inner.position_format=VAT_VEC2;
    // 坐标系和 L2W 由 cfg 传入，不在这里硬编码
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner,
                                             true,
                                             use_array,
                                             SamplerSlot::BaseColor,
                                             use_array);

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[RectTexture2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    FixedTextureSamplerDescriptors samplers;
    build2d::PushBaseUBODescriptors(ubos, &inner);
    build2d::PushBaseSSBODescriptors(ssbos, &inner);
    AddFixedTextureSampler(samplers,
                           SamplerSlot::BaseColor,
                           uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                           use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    if(use_array)
        AddFixedSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

    FixedMaterialDef def {
        "RectTexture2D",  // 统一名称
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        &samplers,
        use_array ? mi_codes : nullptr,
        use_array ? mi_bytes : 0,
    };

    std::string vs = preamble + result.vertex_glsl;
    std::string fs = preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture] CompileCompositorMaterial failed\n");
    return mci;
}

MaterialCreateInfo *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::ScreenRect;
    key.texture_source_mode = TextureSourceMode::Simple;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    return CreateRectTextureVariant(profile, key, cfg);
}

}//namespace hgl::graph::mtl
