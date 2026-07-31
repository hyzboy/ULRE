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
    const bool kRegisteredStandardTextureArrayBmi = []() -> bool
    {
        MaterialDefinition bmi{};
        bmi.definition_name = "StandardTextureArray";
        bmi.builtin_creator_id = static_cast<uint32_t>(BuiltinMaterialCreatorID::StandardTextureArray);
        bmi.source_kind = MaterialDefinitionSourceKind::BuiltIn;
        bmi.with_camera       = true;
        bmi.with_local_to_world = true;
        bmi.with_sky          = true;
        RegisterMaterialDefinition(BuiltinMaterialCreatorID::StandardTextureArray, bmi);
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

static ShaderProgramBuildSpec *CreateStandardTextureArrayImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc)
{
    bc.material_instance = true;

    SkyLightAmbientModel ambient = bc.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors = BuildStandardDescriptors("sampler2DArray");

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedVertexEntry standard_array_vertex[] = {
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Position },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT),    VertexSemantic::TexCoord },
        { ResolveMaterialVertexSemanticFormat(bc.geometry_vertex_format, VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT), VertexSemantic::Normal },
    };

    FixedMaterialDef dynamic_def {
        "StandardTextureArray_v1",
        PrimitiveType::Triangles,
        standard_array_vertex,
        uint32_t(sizeof(standard_array_vertex) / sizeof(standard_array_vertex[0])),
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
        "surface/standard_texturearray_surface.glsl"
    );

    if (!result.success)
    {
        std::fprintf(stderr, "[StandardTextureArray] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderProgramBuildSpec *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        bc);

    if (!mci)
        std::fprintf(stderr, "[StandardTextureArray] CompileCompositorMaterial failed\n");
    return mci;
}

ShaderProgramBuildSpec *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile, const Material3DCreateConfig *cfg)
{
    CompositorMaterialBuildConfig bc = ToCompositorBuildConfig(cfg);
    bc.material_instance = true;
    return CreateStandardTextureArrayImpl(profile, bc);
}


ShaderProgramBuildSpec *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateStandardTextureArrayImpl(profile, ToCompositorBuildConfig3D(request, definition, true));
}
}//namespace hgl::graph::mtl
