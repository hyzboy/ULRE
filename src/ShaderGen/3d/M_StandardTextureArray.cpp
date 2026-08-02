#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/log/Log.h>
#include <vector>

#include "../common/MFSkyLight.h"
#include "../common/VertexBuilderCommon.h"
#include "DefinitionDescriptorBuilder3D.h"

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
        bmi.ssbo_slot_decls   = {{"mtl", SSBOType::ClearCoatSurface}};
        bmi.ubo_requirements  = {UBODescriptorSemantic::ViewportInfo, UBODescriptorSemantic::CameraInfo, UBODescriptorSemantic::SkyInfo};
        bmi.texture_slot_decls = {
            {TextureSlot::BaseColor, GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Normal,    GLSLSamplerType::Sampler2DArray, false},
            {TextureSlot::Roughness, GLSLSamplerType::Sampler2DArray, false},
        };
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

static ShaderProgramBuildSpec *CreateStandardTextureArrayImpl(const contract::PhysicalDeviceProfileLite *profile, CompositorMaterialBuildConfig bc, const MaterialDefinition &definition)
{
    SkyLightAmbientModel ambient = bc.sky_ambient_model;

    std::vector<FixedDescriptorEntry> dynamic_descriptors = Build3DDescriptorsFromDefinition(definition);

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    const vertex_builder_common::VertexSemanticDecl vertex_decls[] = {
        { VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT },
        { VertexSemantic::TexCoord, VK_FORMAT_R32G32_SFLOAT },
        { VertexSemantic::Normal,   VK_FORMAT_R32G32B32_SFLOAT },
    };
    const vertex_builder_common::VertexBuildInput vertex_input {
        PrimitiveType::Triangles,
        vertex_builder_common::VertexTransformIntent::LocalToWorld,
        bc.geometry_vertex_format,
        vertex_decls,
        3
    };
    std::vector<FixedVertexEntry> standard_array_vertex = vertex_builder_common::BuildVertexEntries(vertex_input);

    FixedMaterialDef dynamic_def {
        "StandardTextureArray_v1",
        PrimitiveType::Triangles,
        standard_array_vertex.data(),
        uint32_t(standard_array_vertex.size()),
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
        GLogError("[StandardTextureArray] CompositorAssembler failed: %s",
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
        GLogError("[StandardTextureArray] CompileCompositorMaterial failed");
    return mci;
}

ShaderProgramBuildSpec *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile,const MaterialDefinitionBuildRequest &request,const MaterialDefinition &definition)
{
    return CreateStandardTextureArrayImpl(profile, ToCompositorBuildConfig3D(request, definition), definition);
}
}//namespace hgl::graph::mtl
