#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include "StandardSharedSpec.h"

namespace hgl::graph::mtl{
namespace
{
    const bool kRegisteredStandardBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "Standard";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::Standard);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = true;   // Standard 使用 SkyInfo
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::Standard, bmi);
        return true;
    }();

    constexpr const char mi_codes[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 3;

}

ShaderProgramBuildSpec *CreateStandard(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    // Adapter layer: only difference from StandardTextureArray is "sampler2D".
    std::vector<FixedDescriptorEntry> dynamic_descriptors = BuildStandardDescriptors("sampler2D");

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedVertexEntry standard_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(&cfg_with_mi, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(&cfg_with_mi, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT),    VertexSemantic::TexCoord },
        { ResolveMaterialVertexSemanticFormat(&cfg_with_mi, VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Normal },
    };

    FixedMaterialDef dynamic_def {
        "Standard_v1",
        PrimitiveType::Triangles,
        standard_vertex,
        uint32_t(sizeof(standard_vertex) / sizeof(standard_vertex[0])),
        dynamic_descriptors.data(),
        uint32_t(dynamic_descriptors.size()),
        mi_codes,
        mi_bytes,
    };

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_forward_lit.vert.glsl",
        "compositor/main_forward_lit.frag.glsl",
        "surface/standard_surface.glsl"  // adapter: 2D surface
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl



