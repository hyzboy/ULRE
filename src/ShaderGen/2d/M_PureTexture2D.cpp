#include"../3d/MaterialFactory3DCommon.h"
#include"../3d/Build3DCommon.h"
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/MaterialResourceManifest.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace{

// ── 3D static def template ────────────────────────────────────────────────
constexpr FixedVertexEntry kUnlitTexture3DVertex[] = {
    { VAT_VEC3, VAN::Position },
    { VAT_VEC2, VAN::TexCoord },
};
const UBOSemanticSet  kUnlitTexture3DUBOs  = build3d::MakeViewportCameraUBOs();
const SSBOSemanticSet kUnlitTextureBaseSSBOs = build3d::MakeTransformSSBOs(false);
const StaticMaterialDef kUnlitTexture3DTemplate {
    "UnlitTexture", PrimitiveType::Triangles,
    kUnlitTexture3DVertex, 2,
    &kUnlitTexture3DUBOs, &kUnlitTextureBaseSSBOs, nullptr,
    ShaderDataSchema::None,
};

// ── Unified adapter ───────────────────────────────────────────────────────
static MaterialCreateInfo *UnlitTexture_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key_in,
    MaterialCreateConfig *cfg)
{
    if (!profile || !cfg) return nullptr;

    MaterialVariantKey key = key_in;

    if (cfg->kind == ConfigKind::D2)
    {
        const Material2DCreateConfig *c2 = static_cast<const Material2DCreateConfig *>(cfg);
        const bool use_array = (key.GetTextureSourceMode(SamplerSlot::BaseColor) == TextureSourceMode::Array);

        Material3DCreateConfig cfg3d(c2->prim, IncludeL2W::Without);
        cfg3d.position_format   = VAT_VEC2;
        cfg3d.local_to_world    = c2->local_to_world;
        cfg3d.coord_2d          = c2->coordinate_system;
        cfg3d.preset_name       = c2->preset_name;
        cfg3d.material_instance = use_array;
        MaterialVariantDesc e = *desc; e.coord_2d = c2->coordinate_system;

        FixedVertexEntry v[] = {{ VAT_VEC2, VAN::Position }, { VAT_VEC2, VAN::TexCoord }};
        MaterialResourceManifest mf;
        AddTextureSampler(mf.samplers, SamplerSlot::BaseColor,
                          use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);
        if (use_array) AddSSBODescriptor(mf.ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);
        StaticMaterialDef def {
            "UnlitTexture", cfg3d.prim, v, 2,
            nullptr,
            mf.ssbos.empty()    ? nullptr : &mf.ssbos,
            mf.samplers.empty() ? nullptr : &mf.samplers,
            use_array ? ShaderDataSchema::TextureArrayID : ShaderDataSchema::None
        };
        return CreateFromFixedDef3D("UnlitTexture", profile, def, key, &cfg3d, e);
    }
    else
    {
        auto *c3 = static_cast<Material3DCreateConfig *>(cfg);
        const bool use_array = (key.GetTextureSourceMode(SamplerSlot::BaseColor) == TextureSourceMode::Array);

        c3->local_to_world = true; c3->material_instance = false;

        StaticTextureSamplerDescriptors samplers;
        AddTextureSampler(samplers, SamplerSlot::BaseColor,
                          use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                          0, 0, TextureChannelHint::RGBA);

        SSBOSemanticSet ssbos = kUnlitTextureBaseSSBOs;
        if (use_array) AddSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

        StaticMaterialDef def = kUnlitTexture3DTemplate;
        def.ssbo_descriptors  = &ssbos;
        def.texture_samplers  = &samplers;
        return CreateFromFixedDef3D("UnlitTexture", profile, def, key, c3, *desc);
    }
}
}//anonymous
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(UnlitTexture, "UnlitTexture", hgl::graph::mtl::UnlitTexture_Adapter)
