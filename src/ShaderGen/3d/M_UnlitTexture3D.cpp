#include "MaterialFactory3DCommon.h"
#include "Build3DCommon.h"
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/SamplerSlot.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry UNLIT_TEXTURE_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
    };

    const UBOSemanticSet UNLIT_TEXTURE_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet UNLIT_TEXTURE_3D_BASE_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef UNLIT_TEXTURE_3D_DEF_TEMPLATE {
        "UnlitTexture3D",
        PrimitiveType::Triangles,
        UNLIT_TEXTURE_3D_VERTEX,
        uint32_t(sizeof(UNLIT_TEXTURE_3D_VERTEX) / sizeof(UNLIT_TEXTURE_3D_VERTEX[0])),
        &UNLIT_TEXTURE_3D_UBOS,
        &UNLIT_TEXTURE_3D_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::None,
    };

    static MaterialCreateInfo *CreateUnlitTexture3DFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &routing_key,
        MaterialCreateConfig                      *cfg)
    {
        auto *cfg_3d = static_cast<Material3DCreateConfig *>(cfg);
        if (!cfg_3d)
            return nullptr;

        const TextureSourceMode base_color_mode = routing_key.GetTextureSourceMode(SamplerSlot::BaseColor);
        const bool use_array = (base_color_mode == TextureSourceMode::Array);

        cfg_3d->camera = true;
        cfg_3d->sky = false;
        cfg_3d->local_to_world = true;
        cfg_3d->material_instance = false;

        StaticTextureSamplerDescriptors dynamic_samplers;
        AddTextureSampler(dynamic_samplers,
                          SamplerSlot::BaseColor,
                          use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                          0,
                          0,
                          TextureChannelHint::RGBA);

        SSBOSemanticSet dynamic_ssbos = UNLIT_TEXTURE_3D_BASE_SSBOS;
        if (use_array)
            AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

        StaticMaterialDef dynamic_def = UNLIT_TEXTURE_3D_DEF_TEMPLATE;
        dynamic_def.ssbo_descriptors = &dynamic_ssbos;
        dynamic_def.texture_samplers = &dynamic_samplers;

        return CreateFromFixedDef3D("UnlitTexture3D",
                                    profile,
                                    dynamic_def,
                                    routing_key,
                                    cfg_3d,
                                    *desc);
    }
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(UnlitTexture3D, "UnlitTexture3D", hgl::graph::mtl::CreateUnlitTexture3DFactory)
