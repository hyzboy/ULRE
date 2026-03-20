#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/new/MaterialVariantKey.h>
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
        ? key.GetTextureSourceMode(SamplerName::SamplerSlot::BaseColor)
        : key.texture_source_mode;
    const bool use_array = (mode == TextureSourceMode::Array);

    constexpr const char mi_codes[]="uvec4 id;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4u);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=use_array;
    inner.position_format=VAT_VEC2;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner, true, use_array);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);
    descriptors.push_back({DescriptorSetType::Material,
                           DescriptorKind::TextureSampler,
                           uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                           SamplerName::BaseColor,
                           nullptr,
                           use_array ? "sampler2DArray" : "sampler2D"});

    FixedMaterialDef def {
        use_array ? "RectTexture2DArray" : "RectTexture2D",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        use_array ? mi_codes : nullptr,
        use_array ? mi_bytes : 0,
    };

    std::string vs = preamble + (use_array
        ? "#include \"2d/recttexture2darray.vert.glsl\"\n"
        : "#include \"2d/puretexture2d.vert.glsl\"\n");
    std::string fs = preamble + (use_array
        ? "#include \"2d/recttexture2darray.frag.glsl\"\n"
        : "#include \"2d/puretexture2d.frag.glsl\"\n");

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
    key.SetTextureSourceMode(SamplerName::SamplerSlot::BaseColor, TextureSourceMode::Simple);
    return CreateRectTextureVariant(profile, key, cfg);
}

MaterialCreateInfo *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::ScreenRect;
    key.texture_source_mode = TextureSourceMode::Array;
    key.SetTextureSourceMode(SamplerName::SamplerSlot::BaseColor, TextureSourceMode::Array);
    return CreateRectTextureVariant(profile, key, cfg);
}
}//namespace hgl::graph::mtl
