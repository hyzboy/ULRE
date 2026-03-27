#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderRequireScanner.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include <hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 2;

    constexpr FixedVertexEntry PBR_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex,   VAN::Position },
        { VAT_VEC2, VertexInputRate::Vertex,   VAN::TexCoord },
        { VAT_VEC3, VertexInputRate::Vertex,   VAN::Normal   },
    };

    const FixedUBODescriptors PBR_COLOR_3D_UBOS = {
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::SkyInfo,      uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)},
    };

    const FixedSSBODescriptors PBR_COLOR_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT)},
    };

    const FixedMaterialDef PBR_COLOR_3D_DEF {
        "PBRColor3D",
        PrimitiveType::Triangles,
        PBR_COLOR_3D_VERTEX,
        uint32_t(sizeof(PBR_COLOR_3D_VERTEX)      / sizeof(PBR_COLOR_3D_VERTEX[0])),
        &PBR_COLOR_3D_UBOS,
        &PBR_COLOR_3D_SSBOS,
        nullptr,
        mi_codes,
        mi_bytes,
    };
}//namespace

MaterialCreateInfo *CreatePBRColor3D(const contract::PhysicalDeviceProfileLite *profile, PBRColor3DMaterialCreateConfig *cfg)
{
    if (cfg)
        cfg->material_instance = true;

    // Dynamic descriptor injection for non-Simple sky models
    SkyLightAmbientModel ambient = cfg ? cfg->sky_ambient_model : SkyLightAmbientModel::Simple;

    FixedTextureSamplerDescriptors dynamic_samplers;

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_samplers,
        unused_resources);

    FixedMaterialDef dynamic_def = PBR_COLOR_3D_DEF;
    dynamic_def.texture_samplers        = &dynamic_samplers;

    // Assemble GLSL via VariantRegistry (Standard, Mesh3D, no texture ??color via MI)
    MaterialVariantKey var_key;
    var_key.surface_type = SurfaceType::Standard;
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

    ShaderAutoRequirements auto_requirements;
    std::string require_diagnostics;
    const bool require_ok = CollectShaderAutoRequirements(dynamic_def,
                                                          GetShaderLibraryPath(),
                                                          result.vertex_glsl,
                                                          result.fragment_glsl,
                                                          auto_requirements,
                                                          &require_diagnostics);
    if (!require_ok)
    {
        std::fprintf(stderr, "[PBRColor3D] reflection collection failed:\n%s", require_diagnostics.c_str());
        return nullptr;
    }

    FixedUBODescriptors merged_ubos;
    FixedSSBODescriptors merged_ssbos;
    FixedTextureSamplerDescriptors merged_samplers;
    FixedMaterialDef merged_def = dynamic_def;
    MergeShaderAutoRequirements(dynamic_def,
                                auto_requirements,
                                merged_def,
                                merged_ubos,
                                merged_ssbos,
                                merged_samplers);

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        merged_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[PBRColor3D] CompileCompositorMaterial failed\n");

    return mci;
}

}//namespace hgl::graph::mtl
