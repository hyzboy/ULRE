#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/log/Log.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/mtl/SamplerSlot.h>
#include<cstdio>
#include<vector>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry BILLBOARD_DYNAMIC_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const UBOSemanticSet BILLBOARD_DYNAMIC_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    const SSBOSemanticSet BILLBOARD_DYNAMIC_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };

    constexpr SamplerSlot BILLBOARD_DYNAMIC_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_DYNAMIC_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS) / sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS[0]));

    const StaticMaterialDef BILLBOARD_DYNAMIC_DEF_TEMPLATE {
        "BillboardDynamic",
        PrimitiveType::Triangles,
        BILLBOARD_DYNAMIC_VERTEX,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_VERTEX) / sizeof(BILLBOARD_DYNAMIC_VERTEX[0])),
        &BILLBOARD_DYNAMIC_BASE_UBOS,
        &BILLBOARD_DYNAMIC_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::BillboardSizeUVec2,
    };
    static MaterialCreateInfo *CreateBillboard2DDynamicFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &routing_key,
        MaterialCreateConfig                      *cfg)
    {
        auto *billboard_cfg=static_cast<BillboardMaterialCreateConfig *>(cfg);
        if(!billboard_cfg)
            return(nullptr);

        billboard_cfg->local_to_world=true;
        billboard_cfg->material_instance=true;

        const bool use_array = billboard_cfg->use_texture_array;

        StaticTextureSamplerDescriptors dynamic_samplers;
        for (uint32_t i = 0; i < BILLBOARD_DYNAMIC_TEX_SLOT_COUNT; ++i)
            AddTextureSampler(dynamic_samplers, BILLBOARD_DYNAMIC_TEX_SLOTS[i],
                                   use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                                   0, 0,
                                   billboard_cfg->base_color_channel);

        SSBOSemanticSet dynamic_ssbos = BILLBOARD_DYNAMIC_BASE_SSBOS;
        if (use_array)
            AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

        StaticMaterialDef dynamic_def = BILLBOARD_DYNAMIC_DEF_TEMPLATE;
        dynamic_def.texture_samplers  = &dynamic_samplers;
        dynamic_def.ssbo_descriptors  = &dynamic_ssbos;

        MaterialVariantKey assemble_key = routing_key;
        if (use_array)
            assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

        GLogError( "[BillboardDynamic] use_array=%d  blend=%d  samplerType=%s\n",
            (int)use_array, (int)billboard_cfg->blend_mode,
            use_array ? "Sampler2DArray" : "Sampler2D");

        GLogError(
            "[BillboardDynamic] variant=%s routing_hash=%llu assemble_hash=%llu\n",
            desc->variant_name.c_str(),
            static_cast<unsigned long long>(routing_key.Hash()),
            static_cast<unsigned long long>(assemble_key.Hash()));

        CompositorAssembler assembler;

        auto result = assembler.Assemble(assemble_key, *desc);

        if (!result.success)
        {
            GLogError( "[BillboardDynamic] CompositorAssembler failed: %s\n",
                result.error_message.c_str());
            return nullptr;
        }

        GLogError( "[BillboardDynamic] assemble OK, compiling material...\n");

        MaterialCreateInfo *mci = CompileCompositorMaterial(
            profile,
            dynamic_def,
            result.vertex_glsl,
            result.fragment_glsl,
            billboard_cfg);

        if (!mci)
        {
            GLogError( "[BillboardDynamic] CompileCompositorMaterial failed\n");
        }
        else
        {
            GLogError( "[BillboardDynamic] material created OK\n");
        }

        return mci;
    }
}//namespace
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Billboard2DDynamic, "Billboard2DDynamic", hgl::graph::mtl::CreateBillboard2DDynamicFactory)

