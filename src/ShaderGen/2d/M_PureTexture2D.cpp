#include"../3d/MaterialFactory3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/MaterialResourceManifest.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace{

// ── Unified adapter (2D: position_format==VAT_VEC2, 3D: otherwise) ───────────────
static MaterialCreateInfo *UnlitTexture_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key_in,
    MaterialCreateConfig *cfg)
{
    if (!profile || !cfg) return nullptr;

    auto *c3 = static_cast<Material3DCreateConfig *>(cfg);
    const bool is2D      = (c3->position_format == VAT_VEC2);
    const bool use_array = (key_in.GetTextureSourceMode(SamplerSlot::BaseColor) == TextureSourceMode::Array);

    FixedVertexEntry v2[] = {{ VAT_VEC2, VAN::Position }, { VAT_VEC2, VAN::TexCoord }};
    FixedVertexEntry v3[] = {{ VAT_VEC3, VAN::Position }, { VAT_VEC2, VAN::TexCoord }};

    StaticTextureSamplerDescriptors samplers;
    AddTextureSampler(samplers, SamplerSlot::BaseColor,
                      use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                      0, 0, TextureChannelHint::RGBA);

    SSBOSemanticSet ssbos;
    if (use_array) AddSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

    if (!is2D)
    {
        c3->local_to_world    = true;
        c3->material_instance = false;
    }

    StaticMaterialDef def {
        "UnlitTexture", c3->prim,
        is2D ? v2 : v3, 2,
        nullptr,
        ssbos.empty()    ? nullptr : &ssbos,
        samplers.empty() ? nullptr : &samplers,
        use_array ? ShaderDataSchema::TextureArrayID : ShaderDataSchema::None,
    };
    return CreateFromFixedDef3D("UnlitTexture", profile, def, key_in, c3, *desc);
}
}//anonymous
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(UnlitTexture, "UnlitTexture", hgl::graph::mtl::UnlitTexture_Adapter)
