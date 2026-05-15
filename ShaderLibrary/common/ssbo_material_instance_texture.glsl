// ssbo_material_instance_texture.glsl
// MIT (MaterialInstanceTexture) SSBO: per-instance layer indices for sampler2DArray slots.
// Included by frag_forward_ubo.glsl under #if TEXTURE_ARRAY_MODE.
//
// _ULRE_InitTextureLayerIndices() reads the SSBO once per fragment and writes each
// layer index into the corresponding _tex_layer_X global declared by the emitter.
// This avoids multiple SSBO reads inside per-slot getter functions.

#ifndef SSBO_MIT_GLSL
#define SSBO_MIT_GLSL

struct MaterialInstanceTexture
{
#ifdef TEX_BASECOLOR_ARRAY
    uint BaseColor;
#endif
#ifdef TEX_NORMAL_ARRAY
    uint Normal;
#endif
#ifdef TEX_TANGENT_ARRAY
    uint Tangent;
#endif
#ifdef TEX_METALLIC_ARRAY
    uint Metallic;
#endif
#ifdef TEX_ROUGHNESS_ARRAY
    uint Roughness;
#endif
#ifdef TEX_HEIGHT_ARRAY
    uint Height;
#endif
#ifdef TEX_OPACITY_ARRAY
    uint Opacity;
#endif
#ifdef TEX_TEXT_ARRAY
    uint Text;
#endif
};

#ifndef PERMATERIAL_SET
#define PERMATERIAL_SET 0
#endif
#ifndef MBI_TEXTURE_BINDING
#define MBI_TEXTURE_BINDING 2
#endif

layout(std430, set=PERMATERIAL_SET, binding=MBI_TEXTURE_BINDING) readonly buffer MaterialBindingInstanceTexture
{
    MaterialInstanceTexture tex_id[];
} mbi_texture;

MaterialInstanceTexture GetMaterialInstanceTexture()
{
    return mbi_texture.tex_id[MATERIAL_INSTANCE_ID_OVERRIDE];
}

// Call once at the start of main() to populate all _tex_layer_X globals.
void _ULRE_InitTextureLayerIndices()
{
    MaterialInstanceTexture _m = GetMaterialInstanceTexture();
#ifdef TEX_BASECOLOR_ARRAY
    _tex_layer_BaseColor = _m.BaseColor;
#endif
#ifdef TEX_NORMAL_ARRAY
    _tex_layer_Normal = _m.Normal;
#endif
#ifdef TEX_TANGENT_ARRAY
    _tex_layer_Tangent = _m.Tangent;
#endif
#ifdef TEX_METALLIC_ARRAY
    _tex_layer_Metallic = _m.Metallic;
#endif
#ifdef TEX_ROUGHNESS_ARRAY
    _tex_layer_Roughness = _m.Roughness;
#endif
#ifdef TEX_HEIGHT_ARRAY
    _tex_layer_Height = _m.Height;
#endif
#ifdef TEX_OPACITY_ARRAY
    _tex_layer_Opacity = _m.Opacity;
#endif
#ifdef TEX_TEXT_ARRAY
    _tex_layer_Text = _m.Text;
#endif
}

#endif // SSBO_MIT_GLSL
