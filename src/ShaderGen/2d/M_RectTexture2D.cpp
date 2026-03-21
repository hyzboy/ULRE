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
        ? key.GetTextureSourceMode(SamplerSlot::BaseColor)
        : key.texture_source_mode;
    const bool use_array = (mode == TextureSourceMode::Array);

    // MI 结构：Array 模式需要 MI 数据（用于 MIT SSBO）
    constexpr const char mi_codes[] = "uvec4 id;";
    constexpr const uint32_t mi_bytes = sizeof(math::Vector4u);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=use_array;  // Array 模式需要 MI
    inner.position_format=VAT_VEC2;
    inner.coordinate_system=CoordinateSystem2D::Ortho;  // 使用正交投影
    inner.local_to_world=false;  // 2D材质不使用L2W
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner, true, use_array);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);

    // 根据 mode 动态生成 sampler2D 或 sampler2DArray 描述符
    descriptors.push_back(MakeTextureDescriptorEntry(SamplerSlot::BaseColor,
                                                     uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                                                     mode));

    // Array 模式下添加 MIT SSBO 描述符
    if(use_array)
    {
        descriptors.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::MaterialInstanceTextureID,
                                                       uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)));
    }

    FixedMaterialDef def {
        "RectTexture2D",  // 统一名称
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        use_array ? mi_codes : nullptr,
        use_array ? mi_bytes : 0,
    };

    // 使用统一的着色器文件
    std::string vs = preamble + "#include \"2d/recttexture.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/recttexture.frag.glsl\"\n";

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
