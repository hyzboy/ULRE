#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include <hgl/mtl/MaterialVariantRegistry.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr VertexAttributeSpec PBR_COLOR_3D_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
        { VAN::TexCoord, VAT_VEC2, PF_RG32F  },
        { VAN::Normal,   VAT_VEC3, PF_RGB32F },
    };

    const UBOSemanticSet PBR_COLOR_3D_UBOS = {
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };

    const SSBOSemanticSet PBR_COLOR_3D_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialInstanceID,
        SSBODescriptorSemantic::MaterialInstanceData,
    };

    const StaticMaterialDef PBR_COLOR_3D_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &PBR_COLOR_3D_UBOS,
        &PBR_COLOR_3D_SSBOS,
        nullptr,    // texture_samplers
        nullptr, 0, // mi_glsl_codes / mi_struct_bytes (deprecated)
        InstanceDataLayout::PBRColor,
        PBR_COLOR_3D_VERTEX_SPECS,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX_SPECS) / sizeof(PBR_COLOR_3D_VERTEX_SPECS[0])),
    };
}//namespace

MaterialCreateInfo *CreatePBRColor3D(const contract::PhysicalDeviceProfileLite *profile, PBRColor3DMaterialCreateConfig *cfg)
{
    if (cfg)
        cfg->material_instance = true;

    // Dynamic descriptor injection for non-Simple sky models
    SkyLightAmbientModel ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;
    LightingModel lighting = cfg ? cfg->lighting_model : LightingModel::Lambert;

    StaticTextureSamplerDescriptors dynamic_samplers;

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_samplers,
        unused_resources);

    StaticMaterialDef dynamic_def = PBR_COLOR_3D_DEF;
    dynamic_def.texture_samplers        = &dynamic_samplers;

    // Assemble GLSL via VariantRegistry (Standard, Mesh3D, no texture ??color via MI)
    MaterialVariantKey var_key;
    var_key.SetSurfaceType(SurfaceType::Standard);
    var_key.sky_ambient_model = ambient;
    var_key.lighting_model = lighting;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[PBRColor3D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[PBRColor3D] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[PBRColor3D] CompileCompositorMaterial failed\n");

    return mci;
}

}//namespace hgl::graph::mtl

