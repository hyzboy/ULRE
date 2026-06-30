#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/log/Logger.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include <hgl/mtl/MaterialVariantDesc.h>

#include "../common/MFSkyLight.h"

namespace hgl::graph::mtl{
namespace
{
    static void PrintPBRColorRouteKey(const char *label, const MaterialVariantKey &key)
    {
        GLogError(
            "[PBRColor3D] %s hash=%llu surface=%u geom=%u sky=%u light=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X\n",
            label ? label : "route",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.sky_ambient_model),
            static_cast<unsigned>(key.lighting_model),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
    }

    constexpr FixedVertexEntry PBR_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC3, VAN::Normal   },
    };

    const UBOSemanticSet PBR_COLOR_3D_UBOS = {
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };

    const SSBOSemanticSet PBR_COLOR_3D_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };

    const StaticMaterialDef PBR_COLOR_3D_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        PBR_COLOR_3D_VERTEX,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX)      / sizeof(PBR_COLOR_3D_VERTEX[0])),
        &PBR_COLOR_3D_UBOS,
        &PBR_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::PBRColorParams,
    };

    static MaterialCreateInfo *CreatePBRColor3DFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &,
        MaterialCreateConfig                      *cfg)
    {
        auto *pbr_cfg=static_cast<PBRColor3DMaterialCreateConfig *>(cfg);
        if (pbr_cfg)
            pbr_cfg->material_instance = true;

        SkyLightAmbientModel ambient = pbr_cfg ? pbr_cfg->sky_ambient_model : SkyLightAmbientModel::Simple;
        LightingModel lighting = pbr_cfg ? pbr_cfg->lighting_model : LightingModel::Lambert;

        StaticTextureSamplerDescriptors dynamic_samplers;

        std::vector<const char *> unused_resources;
        ApplySkyLightResourceInjection(
            GetSkyLightResourceInjectionSpec(ambient),
            dynamic_samplers,
            unused_resources);

        StaticMaterialDef dynamic_def = PBR_COLOR_3D_DEF;
        dynamic_def.texture_samplers = &dynamic_samplers;

        MaterialVariantKey var_key;
        var_key.surface_type = SurfaceType::Standard;
        var_key.sky_ambient_model = SkyLightAmbientModel::Simple;
        var_key.lighting_model = lighting;

        for (uint32_t i = 0; i < uint32_t(sizeof(PBR_COLOR_3D_VERTEX) / sizeof(PBR_COLOR_3D_VERTEX[0])); ++i)
            var_key.SetVertexAttribEnabled(PBR_COLOR_3D_VERTEX[i].attrib);

        var_key.sky_ambient_model = ambient;

        PrintPBRColorRouteKey("VariantRegistry resolved route-request", var_key);
        PrintPBRColorRouteKey("VariantRegistry resolved route-final", var_key);
        GLogError(
            "[PBRColor3D] VariantRegistry resolved variant=%s\n",
            desc->variant_name.c_str());

        CompositorAssembler assembler;

        auto result = assembler.Assemble(var_key, *desc);

        if (!result.success)
        {
            GLogError( "[PBRColor3D] CompositorAssembler failed: %s\n",
                result.error_message.c_str());
            return nullptr;
        }

        MaterialCreateInfo *mci = CompileCompositorMaterial(
            profile,
            dynamic_def,
            result.vertex_glsl,
            result.fragment_glsl,
            pbr_cfg);

        if (!mci)
            GLogError( "[PBRColor3D] CompileCompositorMaterial failed\n");

        return mci;
    }
}//namespace

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PBRColor3D, "PBRColor3D", hgl::graph::mtl::CreatePBRColor3DFactory)
